/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include <autoconf.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <sel4bench/flog.h>
#include <sel4bench/sel4bench.h>
#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4utils/sched.h>
#include <sel4utils/sel4_zf_logif.h>
#include <vka/capops.h>
#include <sel4/tfIPC.h>
#include <benchmark.h>
#include <ulscheduler.h>

#define NOPS ""
#define __SWINUM(x) ((x) & 0x00ffffff)

#define NUM_ARGS 5

void
abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void
__arch_putchar(int c)
{
    benchmark_putchar(c);
}

typedef struct task {
    sel4utils_thread_t thread;
    cspacepath_t endpoint_path;
    uint32_t id;
    char args[NUM_ARGS][WORD_STRING_SIZE];
    char *argv[NUM_ARGS];
} task_t;

static task_t tasks[CONFIG_NUM_TASK_SETS + CONFIG_MIN_TASKS];

bool
sched_finished(void *cookie)
{
    size_t *count = (size_t *) cookie;
    (*count)++;
    return (*count > N_RUNS + 1);
}

void
coop_fn(int argc, char **argv)
{
    assert(argc == NUM_ARGS);
    UNUSED int id = (int) atol(argv[0]);
    seL4_CPtr ep = (seL4_CPtr) atol(argv[1]);

    while (1) {
        ZF_LOGD("%d: call\n", id);
        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
    }
}

void
preempt_fn(UNUSED int argc, UNUSED char **argv)
{
    while (true);
}

static void
create_edf_thread(sched_t *sched, env_t *env, task_t *task, int num_tasks, void *fn)
{
    UNUSED int error;
    benchmark_configure_thread(env, seL4_CapNull, seL4_MaxPrio - 1, "edf thread", &task->thread);

    /* allocate a cslot for the minted ep for this process */
    if (vka_cspace_alloc_path(&env->vka, &task->endpoint_path) != 0) {
        ZF_LOGF("Failed to allocate cspace path");
    }

    /* add the thread to the scheduler */
    struct edf_sched_add_tcb_args edf_args = {
        .tcb = task->thread.tcb.cptr,
        .period = NS_IN_MS,
        .budget = NS_IN_MS / num_tasks,
        .slot = task->endpoint_path
    };

    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple),
                                        task->thread.sched_context.cptr,
                                        edf_args.budget / NS_IN_US,
                                        edf_args.period / NS_IN_US, 0);
    ZF_LOGF_IFERR(error, "Failed to configure sched context");

    error = (int) sched_add_tcb(sched, task->thread.sched_context.cptr, (void *) &edf_args);
    ZF_LOGF_IF(error == 0, "Failed to add tcb to scheduler");

    /* set the tfep */
    seL4_CapData_t guard = seL4_CapData_Guard_new(0, seL4_WordBits -
                                                  CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    error = seL4_TCB_SetSpace(task->thread.tcb.cptr, seL4_CapNull,
                              task->endpoint_path.capPtr, simple_get_cnode(&env->simple),
                              guard, simple_get_pd(&env->simple), seL4_NilData);
    ZF_LOGF_IFERR(error, "Failed to set space for tcb");
    /* create args */
    sel4utils_create_word_args(task->args, task->argv, NUM_ARGS, task->id, task->endpoint_path.capPtr);
    /* spawn thread */
    error = sel4utils_start_thread(&task->thread, fn, (void *) NUM_ARGS, (void *) task->argv, true);
    ZF_LOGF_IFERR(error, "Failed to start thread");
}

static void
teardown_thread(vka_t *vka, vspace_t *vspace, task_t *task)
{
    seL4_TCB_Suspend(task->thread.tcb.cptr);
    vka_cnode_delete(&task->endpoint_path);
    vka_cspace_free(vka, task->endpoint_path.capPtr);
    sel4utils_clean_up_thread(vka, vspace, &task->thread);
}

static void
run_edf_benchmark(env_t *env, sched_t *sched, int num_tasks, void *edf_fn, pstimer_t *clock_timer,
                  ccnt_t *results)
{
    size_t count = 0;

    for (int t = 0; t < num_tasks; t++) {
        tasks[t].id = t;
        create_edf_thread(sched, env, &tasks[t], num_tasks, edf_fn);
    }

    timer_start(clock_timer);
    flog_t *flog = flog_init(results, N_RUNS);
    assert(flog != NULL);

    sched_run(sched, sched_finished, &count, (void *) flog);

    for (int t = 0; t < num_tasks; t++) {
        teardown_thread(&env->vka, &env->vspace, &tasks[t]);
    }

    flog_free(flog);
    timer_stop(clock_timer);
    sched_reset(sched);
}

static void
measure_overhead(ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    ulscheduler_results_t *results;
    sched_t *sched;

    env = benchmark_get_env(argc, argv, sizeof(ulscheduler_results_t));
    results = (ulscheduler_results_t *) env->results;

    sel4bench_init();

    measure_overhead(results->overhead);

    /* edf, threads yield immediately */
    sched = sched_new_edf(env->clock_timer, env->timeout_timer, &env->vka, SEL4UTILS_TCB_SLOT, env->ntfn.cptr);
    for (int i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        ZF_LOGD("EDF coop benchmark %d/%d", i + CONFIG_MIN_TASKS, CONFIG_NUM_TASK_SETS + CONFIG_MIN_TASKS - 1);
        run_edf_benchmark(env, sched, i + CONFIG_MIN_TASKS, coop_fn, env->clock_timer->timer, results->edf_coop[i]);
    }

    /* edf, threads rate limited, do not yield */
    for (int i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        ZF_LOGD("EDF preempt benchmark %d/%d", i + CONFIG_MIN_TASKS, CONFIG_NUM_TASK_SETS + CONFIG_MIN_TASKS - 1);
        run_edf_benchmark(env, sched, i + CONFIG_MIN_TASKS, preempt_fn, env->clock_timer->timer,
                          results->edf_preempt[i]);
    }

    /* cfs shared sc coop benchmark */

    /* cfs preemptive non-shared sc */


    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

