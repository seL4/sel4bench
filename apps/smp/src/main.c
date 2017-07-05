/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#include <autoconf.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <utils/time.h>
#include <benchmark.h>
#include <smp.h>

#include "rnorrexp.h"

#define N_ARGS 2
#define ZIGSEED 12345678

static double current_delay_cycle;
static ccnt_t overhead;

typedef struct _per_core_data {
    volatile uint32_t calls_completed;
    char padding[CACHE_LN_SZ - sizeof(uint32_t)];
} per_core_data_t;

struct _pp_threads {
    vka_object_t ep;
    sel4utils_thread_t ping, pong;

    /* arguments to pass to thread's main */
    char thread_args_strings[N_ARGS][WORD_STRING_SIZE];
    char *thread_argv[N_ARGS];

    per_core_data_t pp_ipcs ALIGN(CACHE_LN_SZ);
} pp_threads[CONFIG_MAX_NUM_NODES];

static inline void
wait_for_benchmark(env_t *env)
{
    sel4_timer_handle_single_irq(env->timeout_timer);
    seL4_Wait(env->ntfn.cptr, NULL);
}

static inline void
delay_warmup_period(env_t *env)
{
    for (int i = 0; i < WARMUPS; i++ ) {
        wait_for_benchmark(env);
    }
}

static inline void
ipc_normal_delay(int id)
{
    ccnt_t start, now, delay;

    RESET_CYCLE_COUNTER;
    READ_CYCLE_COUNTER(start);
    delay = OVERHEAD_FIXUP(REXP(id) * current_delay_cycle, overhead);
    READ_CYCLE_COUNTER(now);
    while (now < start + delay) {
        READ_CYCLE_COUNTER(now);
    }
}

void
ping_fn(int argc, char **argv)
{
    assert(argc == N_ARGS);
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    int thread_id = (int) atol(argv[1]);
    volatile uint32_t *calls_completed = &pp_threads[thread_id].pp_ipcs.calls_completed;

    sel4bench_init();
    while (1) {
        ipc_normal_delay(thread_id);
        smp_benchmark_ping(ep);

        (*calls_completed)++;
    }

    /* we would never return... */
}

void
pong_fn(int argc, char **argv)
{
    assert(argc == N_ARGS);
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    int thread_id = (int) atol(argv[1]);

    sel4bench_init();
    while (1) {
        smp_benchmark_pong(ep);
        ipc_normal_delay(thread_id);
    }

    /* we would never return... */
}

static inline void
benchmark_multicore_reset_test(int nr_cores)
{
    for (int i = 0; i < nr_cores; i++) {
        seL4_TCB_Suspend(pp_threads[i].ping.tcb.cptr);
        seL4_TCB_Suspend(pp_threads[i].pong.tcb.cptr);
        pp_threads[i].pp_ipcs.calls_completed = 0;
    }
}

static inline ccnt_t
benchmark_multicore_do_ping_pong(env_t *env, int nr_cores)
{
    ccnt_t total = 0;
    uint32_t start[nr_cores], end[nr_cores];

    delay_warmup_period(env);
    for (int i = 0; i < nr_cores; i++) {
        start[i] = pp_threads[i].pp_ipcs.calls_completed;
    }
    wait_for_benchmark(env);
    for (int i = 0; i < nr_cores; i++) {
        end[i] = pp_threads[i].pp_ipcs.calls_completed;
    }
    for (int i = 0; i < nr_cores; i++) {
        total += (end[i] - start[i]);
    }

    return total;
}

static void
benchmark_multicore_ipc_throughput(env_t *env, smp_results_t *results)
{
    int nr_cores = simple_get_core_count(&env->simple);

    for (int nr_test = 0; nr_test < TESTS; nr_test++) {
        current_delay_cycle = smp_benchmark_params[nr_test].delay;

        for (int core_idx = 0; core_idx < nr_cores; core_idx++) {
            seL4_TCB_Resume(pp_threads[core_idx].ping.tcb.cptr);
            seL4_TCB_Resume(pp_threads[core_idx].pong.tcb.cptr);

            for (int it = 0; it < RUNS; it++) {
                results->benchmarks_result[nr_test][core_idx][it] =
                    benchmark_multicore_do_ping_pong(env, core_idx + 1);
            }
        }

        /* prepare for new test... */
        benchmark_multicore_reset_test(nr_cores);
    }
}

int
main(int argc, char *argv[])
{
    env_t *env;
    UNUSED int error;
    smp_results_t *results;
    int nr_cores;

    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 2 * CONFIG_MAX_NUM_NODES,
        [seL4_EndpointObject] = CONFIG_MAX_NUM_NODES,
    };
    env = benchmark_get_env(argc, argv, sizeof(smp_results_t), object_freq);
    results = (smp_results_t *) env->results;
    nr_cores = simple_get_core_count(&env->simple);
    overhead = smp_benchmark_check_overhead();

    /* initialize random number generator for each core */
    for (int i = 0; i < nr_cores; i++) {
        zigset(i, ZIGSEED + i);
    }

    ZF_LOGF_IF(timer_periodic(env->timeout_timer->timer, NS_IN_S) != 0, "Failed to configure timer\n");
    ZF_LOGF_IF(timer_start(env->timeout_timer->timer) != 0, "Failed to start timer\n");

    for (int i = 0; i < nr_cores; i++) {
        size_t name_sz = strlen("ping") + WORD_STRING_SIZE + 1;

        char ping[name_sz], pong[name_sz];
        snprintf(ping, name_sz, "ping-%i", i);
        snprintf(pong, name_sz, "pong-%i", i);

        /* create ping and pong thread for each core... */
        benchmark_configure_thread(env, 0, seL4_MinPrio, ping, &pp_threads[i].ping);
        benchmark_configure_thread(env, 0, seL4_MinPrio, pong, &pp_threads[i].pong);

        /* create endpoint... */
        error = vka_alloc_endpoint(&env->slab_vka, &pp_threads[i].ep);
        assert(error == seL4_NoError);

        sel4utils_create_word_args(pp_threads[i].thread_args_strings,
                                   pp_threads[i].thread_argv, N_ARGS, pp_threads[i].ep.cptr, i);

        /* prepare ping and pong threads... */
        error = sel4utils_start_thread(&pp_threads[i].ping, (sel4utils_thread_entry_fn) ping_fn,
                                       (void *) N_ARGS, (void *) pp_threads[i].thread_argv, 0);
        assert(error == seL4_NoError);
        error = sel4utils_start_thread(&pp_threads[i].pong, (sel4utils_thread_entry_fn) pong_fn,
                                       (void *) N_ARGS, (void *) pp_threads[i].thread_argv, 0);
        assert(error == seL4_NoError);

        /* prepare thread for pp_ipcs on different cores */
        error = seL4_TCB_SetAffinity(pp_threads[i].ping.tcb.cptr, i);
        assert(!error);
        error = seL4_TCB_SetAffinity(pp_threads[i].pong.tcb.cptr, i);
        assert(!error);
    }

    benchmark_multicore_ipc_throughput(env, results);
    ZF_LOGF_IF(timer_stop(env->timeout_timer->timer) != 0, "Failed to stop timer\n");

    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
