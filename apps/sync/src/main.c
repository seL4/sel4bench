/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4benchsync/gen_config.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <benchmark.h>
#include <sync.h>
#include <sync/bin_sem.h>
#include <sync/condition_var.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>

#define NOPS ""

#define N_WAIT_ARGS 7
#define N_BROADCAST_ARGS 7
#define N_PRODUCER_CONSUMER_ARGS 9

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

int dummy_bin_sem_post(sync_bin_sem_t *lock)
{
    return 0;
}

#define SYNC_GET_ARGUMENTS \
        seL4_CPtr done_ep = (seL4_CPtr) atol(argv[0]);\
        seL4_CPtr block_ep = (seL4_CPtr) atol(argv[1]);\
        sync_bin_sem_t *lock = (sync_bin_sem_t *) atol(argv[2]);\
        sync_cv_t *cv = (sync_cv_t *) atol(argv[3]);\
        int *shared = (int *) atol(argv[4]);\
        ccnt_t *start = (ccnt_t *) atol(argv[5]);\
        ccnt_t *result = (ccnt_t *) atol(argv[6]);\

#define SYNC_WAITER_FUNC(name, wait_fn) \
    void \
    name(int argc, char **argv) \
    { \
        SYNC_GET_ARGUMENTS \
        ccnt_t end;\
        sync_bin_sem_wait(lock);\
        while (*shared == 0) {\
            wait_fn(lock, cv);\
        }\
        SEL4BENCH_READ_CCNT(end);\
        *result = end-(*start);\
        sync_bin_sem_post(lock);\
        seL4_Yield();\
        seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));\
        seL4_Wait(block_ep, NULL);\
    }

SYNC_WAITER_FUNC(waiter_func0, sync_cv_wait)

static helper_func_t bench_waiter_funcs[] = {
    waiter_func0
};

#define SYNC_BROADCAST_FUNC(name, broadcast_fn, post_fn) \
    void \
    name(int argc, char **argv) \
    { \
        SYNC_GET_ARGUMENTS \
        ccnt_t end;\
        sync_bin_sem_wait(lock);\
        *shared = N_WAITERS;\
        SEL4BENCH_READ_CCNT(*start);\
        broadcast_fn(cv);\
        SEL4BENCH_READ_CCNT(end);\
        *result = end-(*start);\
        post_fn(lock);\
        seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));\
        seL4_Wait(block_ep, NULL);\
    }

SYNC_BROADCAST_FUNC(broadcast_func0, sync_cv_broadcast, sync_bin_sem_post);

static helper_func_t bench_broadcast_funcs[] = {
    broadcast_func0
};

void benchmark_broadcast(env_t *env, seL4_CPtr ep, seL4_CPtr block_ep, sync_bin_sem_t *lock,
                         sync_cv_t *cv, sync_results_t *results)
{
    sel4utils_thread_t waiters[N_WAITERS];
    sel4utils_thread_t broadcaster;
    char wait_args_strings[N_WAITERS][N_WAIT_ARGS][WORD_STRING_SIZE];
    char *wait_argv[N_WAITERS][N_WAIT_ARGS];
    char broadcast_args_strings[N_BROADCAST_ARGS][WORD_STRING_SIZE];
    char *broadcast_argv[N_BROADCAST_ARGS];
    int shared;
    ccnt_t start;
    UNUSED int error;

    /* Create some waiter threads and a broadcaster thread */
    for (int i = 0; i != N_WAITERS; ++i)  {
        benchmark_configure_thread(env, 0, seL4_MaxPrio, "waiter", &waiters[i]);
    }
    benchmark_configure_thread(env, 0, seL4_MaxPrio - 1, "broadcaster", &broadcaster);

    for (int j = 0; j < N_BROADCAST_BENCHMARKS; ++j) {
        for (int run = 0; run < N_RUNS; ++run) {
            shared = 0;

            sel4utils_create_word_args(broadcast_args_strings, broadcast_argv,
                                       N_BROADCAST_ARGS, ep, block_ep, lock, cv, &shared, &start,
                                       &(results->broadcast_broadcast_time[j][run]));

            for (int i = 0; i < N_WAITERS; ++i)  {
                sel4utils_create_word_args(wait_args_strings[i], wait_argv[i], N_WAIT_ARGS, ep,
                                           block_ep, lock, cv, &shared, &start,
                                           &(results->broadcast_wait_time[j][i][run]));
                error = sel4utils_start_thread(&waiters[i], (sel4utils_thread_entry_fn) bench_waiter_funcs[j],
                                               (void *) N_WAIT_ARGS, (void *) wait_argv[i], 1);
            }
            assert(error == seL4_NoError);

            error = sel4utils_start_thread(&broadcaster, (sel4utils_thread_entry_fn) bench_broadcast_funcs[j],
                                           (void *) N_BROADCAST_ARGS, (void *) broadcast_argv, 1);
            assert(error == seL4_NoError);

            benchmark_wait_children(ep, "Broadcast bench waiters", N_WAITERS + 1);
        }
    }

    seL4_TCB_Suspend(broadcaster.tcb.cptr);
    for (int i = 0; i < N_WAITERS; i++) {
        seL4_TCB_Suspend(waiters[i].tcb.cptr);
    }
}

#define SYNC_PRODUCER_CONSUMER_FUNC(name, condition, wait_func, work) \
    void \
    name (int argc, char **argv) \
    { \
        seL4_CPtr done_ep = (seL4_CPtr) atol(argv[0]); \
        seL4_CPtr block_ep = (seL4_CPtr) atol(argv[1]); \
        sync_bin_sem_t *lock = (sync_bin_sem_t *) atol(argv[2]); \
        sync_cv_t *wait_cv = (sync_cv_t *) atol(argv[3]); \
        sync_cv_t *signal_cv = (sync_cv_t *) atol(argv[4]); \
        int *fifo_head = (int *) atol(argv[5]); \
        ccnt_t *start = (ccnt_t *) atol(argv[6]); \
        ccnt_t *signal = (ccnt_t *) atol(argv[7]); \
        ccnt_t *result = (ccnt_t *) atol(argv[8]); \
        ccnt_t end; \
        for (int i = 0; i < N_RUNS; i++) { \
            sync_bin_sem_wait(lock); \
            while (condition) { \
                wait_func(lock, wait_cv); \
            } \
            SEL4BENCH_READ_CCNT(end); \
            result[i] = (end-(*start)); \
            work; \
            SEL4BENCH_READ_CCNT(*signal); \
            sync_cv_signal(signal_cv); \
            sync_bin_sem_post(lock); \
        } \
        seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0)); \
        seL4_Wait(block_ep, NULL); \
    }

SYNC_PRODUCER_CONSUMER_FUNC(consumer_func0, (*fifo_head == 0), sync_cv_wait, (*fifo_head)--)

SYNC_PRODUCER_CONSUMER_FUNC(producer_func0, (*fifo_head == FIFO_SIZE), sync_cv_wait, (*fifo_head)++)

static helper_func_t bench_consumer_funcs[] = {
    consumer_func0
};

static helper_func_t bench_producer_funcs[] = {
    producer_func0
};

void benchmark_producer_consumer(env_t *env, seL4_CPtr ep, seL4_CPtr block_ep, sync_bin_sem_t *lock,
                                 sync_cv_t *producer_cv, sync_cv_t *consumer_cv, sync_results_t *results)
{
    sel4utils_thread_t producer, consumer;
    char producer_args_strings[N_PRODUCER_CONSUMER_ARGS][WORD_STRING_SIZE];
    char *producer_argv[N_PRODUCER_CONSUMER_ARGS];
    char consumer_args_strings[N_PRODUCER_CONSUMER_ARGS][WORD_STRING_SIZE];
    char *consumer_argv[N_PRODUCER_CONSUMER_ARGS];
    int UNUSED error;

    /* Create producer consumer threads */
    benchmark_configure_thread(env, 0, seL4_MaxPrio, "producer", &producer);
    benchmark_configure_thread(env, 0, seL4_MaxPrio, "consumer", &consumer);

    for (int j = 0; j != N_PROD_CONS_BENCHMARKS; ++j) {
        int fifo_head = 0;
        ccnt_t producer_signal, consumer_signal;

        sel4utils_create_word_args(producer_args_strings, producer_argv, N_PRODUCER_CONSUMER_ARGS,
                                   ep, block_ep, lock, consumer_cv, producer_cv, &fifo_head,
                                   &producer_signal, &consumer_signal, &(results->consumer_to_producer[j]));

        sel4utils_create_word_args(consumer_args_strings, consumer_argv, N_PRODUCER_CONSUMER_ARGS,
                                   ep, block_ep, lock, producer_cv, consumer_cv, &fifo_head,
                                   &consumer_signal, &producer_signal, &(results->producer_to_consumer[j]));

        error = sel4utils_start_thread(&producer, (sel4utils_thread_entry_fn) bench_producer_funcs[j],
                                       (void *) N_PRODUCER_CONSUMER_ARGS, (void *) producer_argv, 1);
        assert(error == seL4_NoError);

        error = sel4utils_start_thread(&consumer, (sel4utils_thread_entry_fn) bench_consumer_funcs[j],
                                       (void *) N_PRODUCER_CONSUMER_ARGS, (void *) consumer_argv, 1);
        assert(error == seL4_NoError);

        benchmark_wait_children(ep, "Broadcast bench waiters", 2);
    }

    seL4_TCB_Suspend(producer.tcb.cptr);
    seL4_TCB_Suspend(consumer.tcb.cptr);
}

#define SYNC_PRODUCER_CONSUMER_FUNC_EP(name, condition, wait_func, work, direction) \
    void \
    name (int argc, char **argv) \
    { \
        seL4_CPtr done_ep = (seL4_CPtr) atol(argv[0]); \
        seL4_CPtr block_ep = (seL4_CPtr) atol(argv[1]); \
        sync_bin_sem_t *lock = (sync_bin_sem_t *) atol(argv[2]); \
        sync_cv_t *wait_cv = (sync_cv_t *) atol(argv[3]); \
        sync_cv_t *signal_cv = (sync_cv_t *) atol(argv[4]); \
        int *fifo_head = (int *) atol(argv[5]); \
        ccnt_t *start = (ccnt_t *) atol(argv[6]); \
        ccnt_t *signal = (ccnt_t *) atol(argv[7]); \
        sync_results_t *results = (sync_results_t *) atol(argv[8]); \
        ccnt_t end, sum = 0, sum2 = 0; \
        DATACOLLECT_INIT(); \
        for (seL4_Word i = 0; i < N_RUNS; i++) { \
            sync_bin_sem_wait(lock); \
            while (condition) { \
                wait_func(lock, wait_cv); \
            } \
            SEL4BENCH_READ_CCNT(end); \
            DATACOLLECT_GET_SUMS(i, N_IGNORED, *start, end, 0, sum, sum2); \
            work; \
            SEL4BENCH_READ_CCNT(*signal); \
            sync_cv_signal(signal_cv); \
            sync_bin_sem_post(lock); \
        } \
        \
        results->direction##_ep_sum = sum; \
        results->direction##_ep_sum2 = sum2; \
        results->direction##_ep_num = N_RUNS - N_IGNORED; \
        \
        seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0)); \
        seL4_Wait(block_ep, NULL); \
    }

SYNC_PRODUCER_CONSUMER_FUNC_EP(consumer_func_ep, (*fifo_head == 0), sync_cv_wait, (*fifo_head)--, consumer_to_producer)

SYNC_PRODUCER_CONSUMER_FUNC_EP(producer_func_ep, (*fifo_head == FIFO_SIZE), sync_cv_wait, (*fifo_head)++,
                               producer_to_consumer)

void benchmark_producer_consumer_ep(env_t *env, seL4_CPtr ep, seL4_CPtr block_ep, sync_bin_sem_t *lock,
                                    sync_cv_t *producer_cv, sync_cv_t *consumer_cv, sync_results_t *results)
{
    sel4utils_thread_t producer, consumer;
    char producer_args_strings[N_PRODUCER_CONSUMER_ARGS][WORD_STRING_SIZE];
    char *producer_argv[N_PRODUCER_CONSUMER_ARGS];
    char consumer_args_strings[N_PRODUCER_CONSUMER_ARGS][WORD_STRING_SIZE];
    char *consumer_argv[N_PRODUCER_CONSUMER_ARGS];
    int UNUSED error;

    /* Create producer consumer threads */
    benchmark_configure_thread(env, 0, seL4_MaxPrio, "producer", &producer);
    benchmark_configure_thread(env, 0, seL4_MaxPrio, "consumer", &consumer);

    int fifo_head = 0;
    ccnt_t producer_signal, consumer_signal;

    sel4utils_create_word_args(producer_args_strings, producer_argv, N_PRODUCER_CONSUMER_ARGS,
                               ep, block_ep, lock, consumer_cv, producer_cv, &fifo_head,
                               &producer_signal, &consumer_signal, results);

    sel4utils_create_word_args(consumer_args_strings, consumer_argv, N_PRODUCER_CONSUMER_ARGS,
                               ep, block_ep, lock, producer_cv, consumer_cv, &fifo_head,
                               &consumer_signal, &producer_signal, results);

    error = sel4utils_start_thread(&producer, (sel4utils_thread_entry_fn) producer_func_ep,
                                   (void *) N_PRODUCER_CONSUMER_ARGS, (void *) producer_argv, 1);
    assert(error == seL4_NoError);

    error = sel4utils_start_thread(&consumer, (sel4utils_thread_entry_fn) consumer_func_ep,
                                   (void *) N_PRODUCER_CONSUMER_ARGS, (void *) consumer_argv, 1);
    assert(error == seL4_NoError);

    benchmark_wait_children(ep, "Broadcast bench waiters", 2);

    seL4_TCB_Suspend(producer.tcb.cptr);
    seL4_TCB_Suspend(consumer.tcb.cptr);
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = N_WAITERS + 3,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = N_WAITERS + 3,
        [seL4_ReplyObject] = N_WAITERS + 3,
#endif
        [seL4_EndpointObject] = 2,
        [seL4_NotificationObject] = 5,
    };

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(sync_results_t),
              object_freq
          );
}

int main(int argc, char **argv)
{
    UNUSED int error;
    vka_object_t done_ep, block_ep;
    sync_results_t *results;
    sync_bin_sem_t lock;
    sync_cv_t cv, producer_cv, consumer_cv;

    results = (sync_results_t *) env->results;

    sel4bench_init();

    error = vka_alloc_endpoint(&env->slab_vka, &done_ep);
    assert(error == seL4_NoError);

    error = vka_alloc_endpoint(&env->slab_vka, &block_ep);
    assert(error == seL4_NoError);

    /* Broadcast benchmark */
    error = sync_bin_sem_new(&env->slab_vka, &lock, 1);
    assert(error == seL4_NoError);

    error = sync_cv_new(&env->slab_vka, &cv);
    assert(error == seL4_NoError);

    benchmark_broadcast(env, done_ep.cptr, block_ep.cptr, &lock, &cv, results);

    sync_cv_destroy(&env->slab_vka, &cv);
    sync_bin_sem_destroy(&env->slab_vka, &lock);

    /* Producer consumer benchmark */
    error = sync_bin_sem_new(&env->slab_vka, &lock, 1);
    assert(error == seL4_NoError);

    error = sync_cv_new(&env->slab_vka, &producer_cv);
    assert(error == seL4_NoError);

    error = sync_cv_new(&env->slab_vka, &consumer_cv);
    assert(error == seL4_NoError);

    benchmark_producer_consumer(env, done_ep.cptr, block_ep.cptr, &lock,
                                &producer_cv, &consumer_cv, results);

    benchmark_producer_consumer_ep(env, done_ep.cptr, block_ep.cptr, &lock,
                                   &producer_cv, &consumer_cv, results);

    sync_cv_destroy(&env->slab_vka, &producer_cv);
    sync_cv_destroy(&env->slab_vka, &consumer_cv);
    sync_bin_sem_destroy(&env->slab_vka, &lock);

    vka_free_object(&env->slab_vka, &done_ep);
    vka_free_object(&env->slab_vka, &block_ep);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
