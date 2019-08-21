/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <autoconf.h>
#include <sel4benchsignal/gen_config.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <sel4utils/slab.h>

#include <benchmark.h>
#include <sel4benchsupport/signal.h>

#define NOPS ""

#include <arch/signal.h>

#define N_LO_SIGNAL_ARGS 4
#define N_HI_SIGNAL_ARGS 3
#define N_WAIT_ARGS 3
#define MAX_ARGS 4

typedef struct helper_thread {
    sel4utils_thread_t thread;
    char *argv[MAX_ARGS];
    char argv_strings[MAX_ARGS][WORD_STRING_SIZE];
    sel4utils_thread_entry_fn fn;
    seL4_Word argc;
} helper_thread_t;

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void wait_fn(int argc, char **argv)
{

    assert(argc == N_WAIT_ARGS);
    seL4_CPtr ntfn = (seL4_CPtr) atol(argv[0]);
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[1]);
    volatile ccnt_t *end = (volatile ccnt_t *) atol(argv[2]);

    for (int i = 0; i < N_RUNS; i++) {
        DO_REAL_WAIT(ntfn);
        SEL4BENCH_READ_CCNT(*end);
    }

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ntfn, NULL);
}

/* this signal function expects to switch threads (ie wait_fn is higher prio) */
void low_prio_signal_fn(int argc, char **argv)
{
    assert(argc == N_LO_SIGNAL_ARGS);
    seL4_CPtr ntfn = (seL4_CPtr) atol(argv[0]);
    volatile ccnt_t *end = (volatile ccnt_t *) atol(argv[1]);
    ccnt_t *results = (ccnt_t *) atol(argv[2]);
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[3]);

    for (int i = 0; i < N_RUNS; i++) {
        ccnt_t start;
        SEL4BENCH_READ_CCNT(start);
        DO_REAL_SIGNAL(ntfn);
        results[i] = (*end - start);
    }

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ntfn, NULL);
}

void high_prio_signal_fn(int argc, char **argv)
{
    assert(argc == N_HI_SIGNAL_ARGS);
    seL4_CPtr ntfn = (seL4_CPtr) atol(argv[0]);
    signal_results_t *results = (signal_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[2]);

    /* first we run a benchmark where we try and read the cycle counter
     * for individual runs - this may not yield a stable result on all platforms
     * due to pipeline, cache etc */
    for (int i = 0; i < N_RUNS; i++) {
        ccnt_t start, end;
        COMPILER_MEMORY_FENCE();
        SEL4BENCH_READ_CCNT(start);
        DO_REAL_SIGNAL(ntfn);
        SEL4BENCH_READ_CCNT(end);
        COMPILER_MEMORY_FENCE();
        /* record the result */
        results->hi_prio_results[i] = (end - start);
    }

    /* now run an average benchmark and read the perf counters as well */
    seL4_Word n_counters = sel4bench_get_num_counters();
    ccnt_t start = 0;
    ccnt_t end = 0;
    assert(n_counters > 0);
    assert(sel4bench_get_num_generic_counter_chunks(n_counters) > 0);
    for (int j = 0; j < N_RUNS; j++) {
        for (seL4_Word chunk = 0; chunk < sel4bench_get_num_generic_counter_chunks(n_counters); chunk++) {
            COMPILER_MEMORY_FENCE();
            counter_bitfield_t mask = sel4bench_enable_generic_counters(chunk, n_counters);
            SEL4BENCH_READ_CCNT(start);
            for (int i = 0; i < AVERAGE_RUNS; i++) {
                DO_REAL_SIGNAL(ntfn);
            }
            SEL4BENCH_READ_CCNT(end);
            sel4bench_read_and_stop_counters(mask, chunk, n_counters, results->hi_prio_average[j]);
            results->hi_prio_average[j][CYCLE_COUNT_EVENT] = end - start;
            COMPILER_MEMORY_FENCE();
        }
    }

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ntfn, NULL);
}

static void start_threads(helper_thread_t *first, helper_thread_t *second)
{
    UNUSED int error;
    error = sel4utils_start_thread(&first->thread, first->fn, (void *) first->argc, (void *) first->argv, 1);
    assert(error == seL4_NoError);
    error = sel4utils_start_thread(&second->thread, second->fn, (void *) second->argc, (void *) second->argv, 1);
    assert(error == seL4_NoError);
}

static void stop_threads(helper_thread_t *first, helper_thread_t *second)
{
    UNUSED int error;
    error = seL4_TCB_Suspend(first->thread.tcb.cptr);
    assert(error == seL4_NoError);

    error = seL4_TCB_Suspend(second->thread.tcb.cptr);
    assert(error == seL4_NoError);
}

static void benchmark(env_t *env, seL4_CPtr ep, seL4_CPtr ntfn, signal_results_t *results)
{
    helper_thread_t wait = {
        .argc = N_WAIT_ARGS,
        .fn = (sel4utils_thread_entry_fn) wait_fn,
    };

    helper_thread_t signal = {
        .argc = N_LO_SIGNAL_ARGS,
        .fn = (sel4utils_thread_entry_fn) low_prio_signal_fn,
    };

    ccnt_t end;
    UNUSED int error;

    assert(N_LO_SIGNAL_ARGS >= N_HI_SIGNAL_ARGS);

    /* first benchmark signalling to a higher prio thread */
    benchmark_configure_thread(env, ep, seL4_MaxPrio, "wait", &wait.thread);
    benchmark_configure_thread(env, ep, seL4_MaxPrio - 1, "signal", &signal.thread);

    sel4utils_create_word_args(wait.argv_strings, wait.argv, wait.argc, ntfn, ep, (seL4_Word) &end);
    sel4utils_create_word_args(signal.argv_strings, signal.argv, signal.argc, ntfn,
                               (seL4_Word) &end, (seL4_Word) results->lo_prio_results, ep);

    start_threads(&signal, &wait);

    benchmark_wait_children(ep, "children of notification benchmark", 2);

    stop_threads(&signal, &wait);

    seL4_CPtr auth = simple_get_tcb(&env->simple);
    /* now benchmark signalling to a lower prio thread */
    error = seL4_TCB_SetPriority(wait.thread.tcb.cptr, auth, seL4_MaxPrio - 1);
    assert(error == seL4_NoError);

    error = seL4_TCB_SetPriority(signal.thread.tcb.cptr, auth, seL4_MaxPrio);
    assert(error == seL4_NoError);

    /* set our prio down so the waiting thread can get on the endpoint */
    seL4_TCB_SetPriority(SEL4UTILS_TCB_SLOT, auth, seL4_MaxPrio - 2);

    /* change params for high prio signaller */
    signal.fn = (sel4utils_thread_entry_fn) high_prio_signal_fn;
    signal.argc = N_HI_SIGNAL_ARGS;
    sel4utils_create_word_args(signal.argv_strings, signal.argv, signal.argc, ntfn,
                               (seL4_Word) results, ep);

    start_threads(&wait, &signal);

    benchmark_wait_children(ep, "children of notification", 1);

    stop_threads(&wait, &signal);
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

int main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    vka_object_t done_ep, ntfn;
    signal_results_t *results;

    /* configure the slab allocator - we need 2 tcbs, 2 scs, 1 ntfn, 1 ep */
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 2,
        [seL4_EndpointObject] = 1,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 2,
        [seL4_ReplyObject] = 2,
#endif
        [seL4_NotificationObject] = 1,
    };

    env = benchmark_get_env(argc, argv, sizeof(signal_results_t), object_freq);
    results = (signal_results_t *) env->results;

    sel4bench_init();

    error = vka_alloc_endpoint(&env->slab_vka, &done_ep);
    assert(error == seL4_NoError);

    error = vka_alloc_notification(&env->slab_vka, &ntfn);
    assert(error == seL4_NoError);

    /* measure overhead */
    measure_signal_overhead(ntfn.cptr, results->overhead);

    benchmark(env, done_ep.cptr, ntfn.cptr, results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
