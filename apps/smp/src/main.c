/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <smp/gen_config.h>

#include <sel4platsupport/timer.h>
#include <utils/time.h>
#include <benchmark.h>
#include <smp.h>

/* Only used to avoid false cache line sharing between cores. */
#ifdef CONFIG_L1_CACHE_LINE_SIZE_BITS
#define CACHE_LN_SZ BIT(CONFIG_L1_CACHE_LINE_SIZE_BITS)
#else
#define CACHE_LN_SZ 64
#endif
#include "rnorrexp.h"

#define SAMPLE_TIME (100 * NS_IN_MS)

#define N_ARGS 5
#define ZIGSEED 12345678

static double current_delay_cycle;

typedef struct _per_core_data {
    volatile uint32_t calls_completed;
    char padding[CACHE_LN_SZ - sizeof(uint32_t)];
} per_core_data_t;

struct _pp_threads {
    vka_object_t ep;
    sel4utils_thread_t ping, pong;
    sel4utils_checkpoint_t ping_cp, pong_cp;
    vka_object_t ping_sync_ep, pong_sync_ep;

    /* arguments to pass to thread's main */
    char thread_args_strings[N_ARGS][WORD_STRING_SIZE];
    char *thread_argv[N_ARGS];

    per_core_data_t pp_ipcs ALIGN(CACHE_LN_SZ);
} pp_threads[CONFIG_MAX_NUM_NODES];

#if seL4_FastMessageRegisters == 4
#define MSG_REGS NULL, NULL, NULL, NULL
#elif seL4_FastMessageRegisters == 2 // 32-bit x86
#define MSG_REGS NULL, NULL
#elif seL4_FastMessageRegisters == 1 // 32-bit x86 with MCS
#define MSG_REGS NULL
#endif

static inline void smp_benchmark_ping(seL4_CPtr ep)
{
    seL4_CallWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), MSG_REGS);
}

static inline void smp_benchmark_pong(seL4_CPtr ep, seL4_CPtr reply)
{
#if CONFIG_KERNEL_MCS
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, MSG_REGS, reply);
#else
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, MSG_REGS);
#endif
}

static inline void wait_for_benchmark(env_t *env)
{
    seL4_Word badge;
    seL4_Wait(env->ntfn.cptr, &badge);
    sel4platsupport_irq_handle(&env->io_ops.irq_ops, env->ntfn_id, badge);
}

static inline void ipc_normal_delay(int id)
{
    ccnt_t start, now, delay;

    delay = current_delay_cycle;
    start = sel4bench_get_cycle_count();
    do {
        now = sel4bench_get_cycle_count();
    } while (now - start < delay);
}

void *ping_fn(int argc, char **argv, void *x)
{
    assert(argc == N_ARGS);
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    int thread_id = (int) atol(argv[1]);
    seL4_CPtr sync_ep = (seL4_CPtr) atol(argv[3]);
    volatile uint32_t *calls_completed = &pp_threads[thread_id].pp_ipcs.calls_completed;

    sel4bench_init();

    /* sync with main thread so we are guaranteed to be waiting when
       sel4utils_checkpoint_thread is called on us */
    seL4_Call(sync_ep, seL4_MessageInfo_new(0, 0, 0, 0));

    while (1) {
        ipc_normal_delay(thread_id);
        smp_benchmark_ping(ep);

        (*calls_completed)++;
    }

    /* we would never return... */
}

void *pong_fn(int argc, char **argv, void *x)
{
    assert(argc == N_ARGS);
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    int thread_id = (int) atol(argv[1]);
    seL4_CPtr reply = (seL4_CPtr) atol(argv[2]);
    seL4_CPtr sync_ep = (seL4_CPtr) atol(argv[4]);

    sel4bench_init();

    /* sync with main thread so we are guaranteed to be waiting when
       sel4utils_checkpoint_thread is called on us */
    seL4_Call(sync_ep, seL4_MessageInfo_new(0, 0, 0, 0));

    while (1) {
        smp_benchmark_pong(ep, reply);
        ipc_normal_delay(thread_id);
    }

    /* we would never return... */
}

/* Sync with a benchmark thread at its sync endpoint and checkpoint it. */
static void sync_and_checkpoint(sel4utils_thread_t *thread, sel4utils_checkpoint_t *checkpoint,
                                seL4_CPtr sync_ep, seL4_CPtr reply,
                                const char *name, int nr_test, int core_idx)
{
    api_recv(sync_ep, NULL, reply);

    int error = sel4utils_checkpoint_thread(thread, checkpoint, false);
    ZF_LOGF_IF(error != 0, "Failed to checkpoint %s (test %d, core %d)", name, nr_test, core_idx);

    api_reply(reply, seL4_MessageInfo_new(0, 0, 0, 0));
}

static inline void benchmark_multicore_reset_test(int nr_cores)
{
    int error;
    for (int i = 0; i < nr_cores; i++) {
        seL4_TCB_Suspend(pp_threads[i].ping.tcb.cptr);
        seL4_TCB_Suspend(pp_threads[i].pong.tcb.cptr);
        pp_threads[i].pp_ipcs.calls_completed = 0;

        /* restore ping and pong to synchronisation point in the benchmark */
        sel4utils_checkpoint_restore(&pp_threads[i].pong_cp, true, false);
        sel4utils_checkpoint_restore(&pp_threads[i].ping_cp, true, false);
    }
}

static inline ccnt_t benchmark_multicore_do_ping_pong(env_t *env, int nr_cores)
{
    ccnt_t total = 0;
    uint32_t start[nr_cores], end[nr_cores];

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

    /* normalise throughput to ipc/sec, force 64 bit against mult overflow */
    return ((uint64_t) total * NS_IN_S) / SAMPLE_TIME;
}

static void benchmark_multicore_ipc_throughput(env_t *env, smp_results_t *results, seL4_CPtr sync_reply)
{
    int nr_cores = simple_get_core_count(&env->simple);

    /* Make future wait times more deterministic. */
    wait_for_benchmark(env);

    for (int nr_test = 0; nr_test < TESTS; nr_test++) {
        current_delay_cycle = smp_benchmark_params[nr_test].delay;

        for (int core_idx = 0; core_idx < nr_cores; core_idx++) {
            seL4_TCB_Resume(pp_threads[core_idx].ping.tcb.cptr);
            seL4_TCB_Resume(pp_threads[core_idx].pong.tcb.cptr);

            /* checkpoint ping+pong threads at their sync points, and release them into
             * benchmark loop */
            sync_and_checkpoint(&pp_threads[core_idx].ping, &pp_threads[core_idx].ping_cp,
                                pp_threads[core_idx].ping_sync_ep.cptr, sync_reply,
                                "ping", nr_test, core_idx);
            sync_and_checkpoint(&pp_threads[core_idx].pong, &pp_threads[core_idx].pong_cp,
                                pp_threads[core_idx].pong_sync_ep.cptr, sync_reply,
                                "pong", nr_test, core_idx);

            /* Synchronise to start of timer period to measure the correct amount of time.
             * This delay also lets the threads run and acts as a warm-up period. */
            wait_for_benchmark(env);

            for (int it = 0; it < RUNS; it++) {
                results->benchmarks_result[nr_test][core_idx][it] =
                    benchmark_multicore_do_ping_pong(env, core_idx + 1);
            }
        }

        /* prepare for new test... */
        benchmark_multicore_reset_test(nr_cores);
    }
}

int main(int argc, char *argv[])
{
    env_t *env;
    UNUSED int error;
    smp_results_t *results;
    int nr_cores;

    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 2 * CONFIG_MAX_NUM_NODES,
        /* one benchmark endpoint and two checkpoint sync endpoints per core */
        [seL4_EndpointObject] = 3 * CONFIG_MAX_NUM_NODES,
    };
    env = benchmark_get_env(argc, argv, sizeof(smp_results_t), object_freq);
    benchmark_init_timer(env);
    results = (smp_results_t *) env->results;
    nr_cores = simple_get_core_count(&env->simple);

    /* reply object for receiving the checkpoint sync calls (MCS only) */
    seL4_CPtr sync_reply = seL4_CapNull;
#ifdef CONFIG_KERNEL_MCS
    vka_object_t sync_reply_obj = {0};
    error = vka_alloc_reply(&env->slab_vka, &sync_reply_obj);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate reply object for sync checkpoint");
    sync_reply = sync_reply_obj.cptr;
#endif

    /* initialize random number generator for each core */
    for (int i = 0; i < nr_cores; i++) {
        zigset(i, ZIGSEED + i);
    }

    ZF_LOGF_IF(ltimer_reset(&env->ltimer) != 0, "Failed to start timer\n");
    ZF_LOGF_IF(ltimer_set_timeout(&env->ltimer, SAMPLE_TIME, TIMEOUT_PERIODIC) != 0, "Failed to configure timer\n");

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

        /* create endpoints for the checkpoint sync with ping and pong */
        error = vka_alloc_endpoint(&env->slab_vka, &pp_threads[i].ping_sync_ep);
        ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate sync endpoint for ping");
        error = vka_alloc_endpoint(&env->slab_vka, &pp_threads[i].pong_sync_ep);
        ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate sync endpoint for pong");

        sel4utils_create_word_args(pp_threads[i].thread_args_strings,
                                   pp_threads[i].thread_argv, N_ARGS, pp_threads[i].ep.cptr, i,
                                   pp_threads[i].ping.reply.cptr, pp_threads[i].ping_sync_ep.cptr,
                                   pp_threads[i].pong_sync_ep.cptr);

        /* prepare ping and pong threads... */
        error = sel4utils_start_thread(&pp_threads[i].ping, (sel4utils_thread_entry_fn) ping_fn,
                                       (void *) N_ARGS, (void *) pp_threads[i].thread_argv, 0);
        assert(error == seL4_NoError);
        error = sel4utils_start_thread(&pp_threads[i].pong, (sel4utils_thread_entry_fn) pong_fn,
                                       (void *) N_ARGS, (void *) pp_threads[i].thread_argv, 0);
        assert(error == seL4_NoError);

        /* prepare thread for pp_ipcs on different cores */
        sched_params_t params = {0};
#ifdef CONFIG_KERNEL_MCS
        params = sched_params_round_robin(params, &env->simple, i, CONFIG_BOOT_THREAD_TIME_SLICE * US_IN_MS);
#else
        params.core = i;
#endif

        error = sel4utils_set_sched_affinity(&pp_threads[i].ping, params);
        assert(!error);
        error = sel4utils_set_sched_affinity(&pp_threads[i].pong, params);
        assert(!error);
    }

    benchmark_multicore_ipc_throughput(env, results, sync_reply);
    ZF_LOGF_IF(ltimer_reset(&env->ltimer) != 0, "Failed to stop timer\n");

    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
