/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <autoconf.h>
#include <sel4benchscheduler/gen_config.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>

#include <benchmark.h>
#include <scheduler.h>

#define NOPS ""

#include <arch/signal.h>
#define N_LOW_ARGS 5
#define N_HIGH_ARGS 4
#define N_YIELD_ARGS 2

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void high_fn(int argc, char **argv)
{

    assert(argc == N_HIGH_ARGS);
    seL4_CPtr produce = (seL4_CPtr) atol(argv[0]);
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[1]);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[2]);
    seL4_CPtr consume = (seL4_CPtr) atol(argv[3]);

    for (int i = 0; i < N_RUNS; i++) {
        DO_REAL_SIGNAL(produce);
        /* we're running at high prio, read the cycle counter */
        SEL4BENCH_READ_CCNT(*start);
        DO_REAL_WAIT(consume);
    }

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(produce, NULL);
}

void low_fn(int argc, char **argv)
{
    assert(argc == N_LOW_ARGS);
    seL4_CPtr produce = (seL4_CPtr) atol(argv[0]);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[1]);
    ccnt_t *results = (ccnt_t *) atol(argv[2]);
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[3]);
    seL4_CPtr consume = (seL4_CPtr) atol(argv[4]);

    for (int i = 0; i < N_RUNS; i++) {
        ccnt_t end;
        DO_REAL_WAIT(produce);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - *start);
        DO_REAL_SIGNAL(consume);
    }

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(produce, NULL);
}

static void yield_fn(int argc, char **argv)
{

    assert(argc == N_YIELD_ARGS);

    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    volatile ccnt_t *end = (volatile ccnt_t *) atol(argv[1]);

    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(*end);
        seL4_Yield();
    }

    seL4_Send(ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void benchmark_yield(seL4_CPtr ep, ccnt_t *results, volatile ccnt_t *end)
{
    ccnt_t start;
    /* run the benchmark */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        seL4_Yield();
        results[i] = (*end - start);
    }

    benchmark_wait_children(ep, "yielder", 1);
}

static void benchmark_yield_ep(seL4_CPtr ep, ccnt_t overhead, ccnt_t *result_sum, ccnt_t *result_sum2,
                               ccnt_t *result_num, volatile ccnt_t *end)
{
    ccnt_t start, sum = 0, sum2 = 0;
    DATACOLLECT_INIT();
    /* run the benchmark */
    for (seL4_Word i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        seL4_Yield();
        DATACOLLECT_GET_SUMS(i, N_IGNORED, start, *end, overhead, sum, sum2);
    }

    *result_sum = sum;
    *result_sum2 = sum2;
    *result_num = N_RUNS - N_IGNORED;

    benchmark_wait_children(ep, "yielder", 1);
}

static void benchmark_yield_thread(env_t *env, seL4_CPtr ep, ccnt_t *results)
{
    sel4utils_thread_t thread;
    volatile ccnt_t end;
    char args_strings[N_YIELD_ARGS][WORD_STRING_SIZE];
    char *argv[N_YIELD_ARGS];

    benchmark_configure_thread(env, ep, seL4_MaxPrio, "yielder", &thread);
    sel4utils_create_word_args(args_strings, argv, N_YIELD_ARGS, ep, (seL4_Word) &end);
    sel4utils_start_thread(&thread, (sel4utils_thread_entry_fn) yield_fn, (void *) N_YIELD_ARGS, (void *) argv, 1);

    benchmark_yield(ep, results, &end);
    seL4_TCB_Suspend(thread.tcb.cptr);
}

static void benchmark_yield_thread_ep(env_t *env, seL4_CPtr ep, scheduler_results_t *results)
{
    sel4utils_thread_t thread;
    volatile ccnt_t end;
    char args_strings[N_YIELD_ARGS][WORD_STRING_SIZE];
    char *argv[N_YIELD_ARGS];

    benchmark_configure_thread(env, ep, seL4_MaxPrio, "yielder", &thread);
    sel4utils_create_word_args(args_strings, argv, N_YIELD_ARGS, ep, (seL4_Word) &end);
    sel4utils_start_thread(&thread, (sel4utils_thread_entry_fn) yield_fn, (void *) N_YIELD_ARGS, (void *) argv, 1);

    benchmark_yield_ep(ep, results->overhead_ccnt_min, &results->thread_yield_ep_sum, &results->thread_yield_ep_sum2,
                       &results->thread_yield_ep_num, &end);
    seL4_TCB_Suspend(thread.tcb.cptr);
}

static void benchmark_yield_process(env_t *env, seL4_CPtr ep, ccnt_t *results)
{
    sel4utils_process_t process;
    void *start;
    void *remote_start;
    seL4_CPtr remote_ep;
    char args_strings[N_YIELD_ARGS][WORD_STRING_SIZE];
    char *argv[N_YIELD_ARGS];
    UNUSED int error;
    cspacepath_t path;

    /* allocate a page to share for the start cycle count */
    start = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    assert(start != NULL);

    benchmark_shallow_clone_process(env, &process, seL4_MaxPrio, yield_fn, "yield process");

    /* share memory for shared variable */
    remote_start = vspace_share_mem(&env->vspace, &process.vspace, start, 1, seL4_PageBits,
                                    seL4_AllRights, 1);
    assert(remote_start != NULL);

    /* copy ep cap */
    vka_cspace_make_path(&env->slab_vka, ep, &path);
    remote_ep = sel4utils_copy_path_to_process(&process, path);
    assert(remote_ep != seL4_CapNull);

    sel4utils_create_word_args(args_strings, argv, N_YIELD_ARGS, remote_ep, (seL4_Word) remote_start);

    error = benchmark_spawn_process(&process, &env->slab_vka, &env->vspace, N_YIELD_ARGS, argv, 1);
    assert(error == seL4_NoError);

    benchmark_yield(ep, results, (volatile ccnt_t *) start);
    seL4_TCB_Suspend(process.thread.tcb.cptr);
}

static void benchmark_yield_process_ep(env_t *env, seL4_CPtr ep, scheduler_results_t *results)
{
    sel4utils_process_t process;
    void *start;
    void *remote_start;
    seL4_CPtr remote_ep;
    char args_strings[N_YIELD_ARGS][WORD_STRING_SIZE];
    char *argv[N_YIELD_ARGS];
    UNUSED int error;
    cspacepath_t path;

    /* allocate a page to share for the start cycle count */
    start = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    assert(start != NULL);

    benchmark_shallow_clone_process(env, &process, seL4_MaxPrio, yield_fn, "yield process");

    /* share memory for shared variable */
    remote_start = vspace_share_mem(&env->vspace, &process.vspace, start, 1, seL4_PageBits,
                                    seL4_AllRights, 1);
    assert(remote_start != NULL);

    /* copy ep cap */
    vka_cspace_make_path(&env->slab_vka, ep, &path);
    remote_ep = sel4utils_copy_path_to_process(&process, path);
    assert(remote_ep != seL4_CapNull);

    sel4utils_create_word_args(args_strings, argv, N_YIELD_ARGS, remote_ep, (seL4_Word) remote_start);

    error = benchmark_spawn_process(&process, &env->slab_vka, &env->vspace, N_YIELD_ARGS, argv, 1);
    assert(error == seL4_NoError);

    benchmark_yield_ep(ep, results->overhead_ccnt_min, &results->process_yield_ep_sum, &results->process_yield_ep_sum2,
                       &results->process_yield_ep_num, (volatile ccnt_t *) start);
    seL4_TCB_Suspend(process.thread.tcb.cptr);
}

static void benchmark_prio_threads(env_t *env, seL4_CPtr ep, seL4_CPtr produce, seL4_CPtr consume,
                                   ccnt_t results[N_PRIOS][N_RUNS])
{
    sel4utils_thread_t high, low;
    char high_args_strings[N_HIGH_ARGS][WORD_STRING_SIZE];
    char *high_argv[N_HIGH_ARGS];
    char low_args_strings[N_LOW_ARGS][WORD_STRING_SIZE];
    char *low_argv[N_LOW_ARGS];
    ccnt_t start;
    UNUSED int error;

    benchmark_configure_thread(env, ep, seL4_MinPrio, "high", &high);
    benchmark_configure_thread(env, ep, seL4_MinPrio, "low", &low);

    sel4utils_create_word_args(high_args_strings, high_argv, N_HIGH_ARGS, produce,
                               ep, (seL4_Word) &start, consume);

    for (int i = 0; i < N_PRIOS; i++) {
        uint8_t prio = gen_next_prio(i);
        error = seL4_TCB_SetPriority(high.tcb.cptr, simple_get_tcb(&env->simple), prio);
        assert(error == seL4_NoError);

        sel4utils_create_word_args(low_args_strings, low_argv, N_LOW_ARGS, produce,
                                   (seL4_Word) &start, (seL4_Word) &results[i], ep, consume);

        error = sel4utils_start_thread(&low, (sel4utils_thread_entry_fn) low_fn, (void *) N_LOW_ARGS, (void *) low_argv, 1);
        assert(error == seL4_NoError);
        error = sel4utils_start_thread(&high, (sel4utils_thread_entry_fn) high_fn, (void *) N_HIGH_ARGS, (void *) high_argv, 1);
        assert(error == seL4_NoError);

        benchmark_wait_children(ep, "children of scheduler benchmark", 2);
    }

    seL4_TCB_Suspend(high.tcb.cptr);
    seL4_TCB_Suspend(low.tcb.cptr);
}

static void benchmark_prio_processes(env_t *env, seL4_CPtr ep, seL4_CPtr produce, seL4_CPtr consume,
                                     ccnt_t results[N_PRIOS][N_RUNS])
{
    sel4utils_process_t high;
    sel4utils_thread_t low;
    char high_args_strings[N_HIGH_ARGS][WORD_STRING_SIZE];
    char *high_argv[N_HIGH_ARGS];
    char low_args_strings[N_LOW_ARGS][WORD_STRING_SIZE];
    char *low_argv[N_LOW_ARGS];
    void *start, *remote_start;
    seL4_CPtr remote_ep, remote_produce, remote_consume;
    UNUSED int error;
    cspacepath_t path;

    /* allocate a page to share for the start cycle count */
    start = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    assert(start != NULL);

    benchmark_shallow_clone_process(env, &high, seL4_MinPrio, high_fn, "high");
    /* run low in the same thread as us so we don't have to copy the results across */
    benchmark_configure_thread(env, ep, seL4_MinPrio, "low", &low);

    /* share memory for shared variable */
    remote_start = vspace_share_mem(&env->vspace, &high.vspace, start, 1, seL4_PageBits, seL4_AllRights, 1);
    assert(remote_start != NULL);

    /* copy ep cap */
    vka_cspace_make_path(&env->slab_vka, ep, &path);
    remote_ep = sel4utils_copy_path_to_process(&high, path);
    assert(remote_ep != seL4_CapNull);

    /* copy ntfn cap */
    vka_cspace_make_path(&env->slab_vka, produce, &path);
    remote_produce = sel4utils_copy_path_to_process(&high, path);
    assert(remote_produce != seL4_CapNull);

    /* copy ntfn cap */
    vka_cspace_make_path(&env->slab_vka, consume, &path);
    remote_consume = sel4utils_copy_path_to_process(&high, path);
    assert(remote_consume != seL4_CapNull);

    sel4utils_create_word_args(high_args_strings, high_argv, N_HIGH_ARGS, remote_produce,
                               remote_ep, (seL4_Word) remote_start, remote_consume);

    for (int i = 0; i < N_PRIOS; i++) {
        uint8_t prio = gen_next_prio(i);
        error = seL4_TCB_SetPriority(high.thread.tcb.cptr, simple_get_tcb(&env->simple), prio);
        assert(error == 0);

        sel4utils_create_word_args(low_args_strings, low_argv, N_LOW_ARGS, produce,
                                   (seL4_Word) start, (seL4_Word) &results[i], ep, consume);

        error = sel4utils_start_thread(&low, (sel4utils_thread_entry_fn) low_fn, (void *) N_LOW_ARGS, (void *) low_argv, 1);
        assert(error == seL4_NoError);

        error = benchmark_spawn_process(&high, &env->slab_vka, &env->vspace, N_HIGH_ARGS, high_argv, 1);
        assert(error == seL4_NoError);

        benchmark_wait_children(ep, "children of scheduler benchmark", 2);
    }

    seL4_TCB_Suspend(high.thread.tcb.cptr);
    seL4_TCB_Suspend(low.tcb.cptr);
}

void measure_signal_overhead(seL4_CPtr ntfn, ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_SIGNAL(ntfn);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

void measure_yield_overhead(ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

void benchmark_set_prio_average(ccnt_t results[N_RUNS][NUM_AVERAGE_EVENTS], seL4_CPtr auth)
{
    seL4_Word n_counters = sel4bench_get_num_counters();
    ccnt_t start = 0;
    ccnt_t end = 0;

    for (int j = 0; j < N_RUNS; j++) {
        for (seL4_Word chunk = 0; chunk < sel4bench_get_num_generic_counter_chunks(n_counters); chunk++) {
            COMPILER_MEMORY_FENCE();
            counter_bitfield_t mask = sel4bench_enable_generic_counters(chunk, n_counters);
            SEL4BENCH_READ_CCNT(start);
            for (int i = 0; i < AVERAGE_RUNS; i++) {
                /* set prio on self always triggers a reschedule */
                seL4_TCB_SetPriority(SEL4UTILS_TCB_SLOT, auth, seL4_MaxPrio);
            }
            SEL4BENCH_READ_CCNT(end);
            sel4bench_read_and_stop_counters(mask, chunk, n_counters, results[j]);
            COMPILER_MEMORY_FENCE();
            results[j][CYCLE_COUNT_EVENT] = end - start;
        }
    }

}

void benchmark_yield_average(ccnt_t results[N_RUNS][NUM_AVERAGE_EVENTS])
{
    seL4_Word n_counters = sel4bench_get_num_counters();
    ccnt_t start = 0;
    ccnt_t end = 0;

    for (int j = 0; j < N_RUNS; j++) {
        for (seL4_Word chunk = 0; chunk < sel4bench_get_num_generic_counter_chunks(n_counters); chunk++) {
            COMPILER_MEMORY_FENCE();
            counter_bitfield_t mask = sel4bench_enable_generic_counters(chunk, n_counters);
            SEL4BENCH_READ_CCNT(start);
            for (int i = 0; i < AVERAGE_RUNS; i++) {
                seL4_Yield();
            }
            SEL4BENCH_READ_CCNT(end);
            sel4bench_read_and_stop_counters(mask, chunk, n_counters, results[j]);
            COMPILER_MEMORY_FENCE();
            results[j][CYCLE_COUNT_EVENT] = end - start;
        }
    }
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 6,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 6,
        [seL4_ReplyObject] = 6,
#endif
        [seL4_EndpointObject] = 1,
        [seL4_NotificationObject] = 2,
    };

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(scheduler_results_t),
              object_freq
          );
}

int main(int argc, char **argv)
{
    UNUSED int error;
    vka_object_t done_ep, produce, consume;
    scheduler_results_t *results;

    results = (scheduler_results_t *) env->results;

    sel4bench_init();

    error = vka_alloc_endpoint(&env->slab_vka, &done_ep);
    assert(error == seL4_NoError);

    error = vka_alloc_notification(&env->slab_vka, &produce);
    assert(error == seL4_NoError);

    error = vka_alloc_notification(&env->slab_vka, &consume);
    assert(error == seL4_NoError);

    /* measure overhead */
    measure_signal_overhead(produce.cptr, results->overhead_signal);
    measure_yield_overhead(results->overhead_ccnt);

    /* extract the minimum overhead for early processing benchmarks */
    results->overhead_ccnt_min = getMinOverhead(results->overhead_ccnt, N_RUNS);

    benchmark_prio_threads(env, done_ep.cptr, produce.cptr, consume.cptr,
                           results->thread_results);
    benchmark_prio_processes(env, done_ep.cptr, produce.cptr, consume.cptr,
                             results->process_results);
    benchmark_set_prio_average(results->set_prio_average, simple_get_tcb(&env->simple));

    /* thread yield benchmarks */
    benchmark_yield_thread(env, done_ep.cptr, results->thread_yield);
    benchmark_yield_thread_ep(env, done_ep.cptr, results);
    benchmark_yield_process(env, done_ep.cptr, results->process_yield);
    benchmark_yield_process_ep(env, done_ep.cptr, results);
    benchmark_yield_average(results->average_yield);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
