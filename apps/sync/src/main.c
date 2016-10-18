/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include <autoconf.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>

#include <benchmark.h>
#include <sync.h>
#include <sync/bin_sem.h>
#include <sync/condition_var.h>

#define NOPS ""

#define N_WAIT_ARGS 6
#define N_BROADCAST_ARGS 5

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

void
waiter_func(int argc, char **argv)
{
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[0]);
    seL4_CPtr block_ep = (seL4_CPtr) atol(argv[1]);
    sync_bin_sem_t *lock = (sync_bin_sem_t *) atol(argv[2]);
    sync_cv_t *cv = (sync_cv_t *) atol(argv[3]);
    int *shared = (int *) atol(argv[4]);
    ccnt_t *result = (ccnt_t *) atol(argv[5]);
    ccnt_t start, end;

    SEL4BENCH_READ_CCNT(start);
    sync_bin_sem_wait(lock);
    while (*shared == 0) {
        sync_cv_wait(lock, cv);
    }
    SEL4BENCH_READ_CCNT(end);
    *result = end-start;

    (*shared)--;

    sync_bin_sem_post(lock);

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    seL4_Recv(block_ep, NULL);
}

void
broadcast_func(int argc, char **argv)
{
    seL4_CPtr done_ep = (seL4_CPtr) atol(argv[0]);
    seL4_CPtr block_ep = (seL4_CPtr) atol(argv[1]);
    sync_bin_sem_t *lock = (sync_bin_sem_t *) atol(argv[2]);
    sync_cv_t *cv = (sync_cv_t *) atol(argv[3]);
    int *shared = (int *) atol(argv[4]);
    ccnt_t start, end;

    sync_bin_sem_wait(lock);

    *shared = N_WAITERS;

    sync_cv_broadcast(cv);
    sync_bin_sem_post(lock);

    /* signal completion */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    seL4_Recv(block_ep, NULL);
}

void
benchmark_broadcast(env_t *env, seL4_CPtr ep, seL4_CPtr block_ep, sync_bin_sem_t *lock, 
        sync_cv_t *cv, sync_results_t *results)
{
    sel4utils_thread_t waiters[N_WAITERS];
    sel4utils_thread_t broadcaster;
    char wait_args_strings[N_WAITERS][N_WAIT_ARGS][WORD_STRING_SIZE];
    char *wait_argv[N_WAITERS][N_WAIT_ARGS];
    char broadcast_args_strings[N_BROADCAST_ARGS][WORD_STRING_SIZE];
    char *broadcast_argv[N_BROADCAST_ARGS];
    int shared = 0;
    UNUSED int error;

    /* Create some waiter threads and a broadcaster thread */
    for (int i = 0; i != N_WAITERS; ++i)  {
        benchmark_configure_thread(env, 0, seL4_MaxPrio, "waiter", &waiters[i]);
    }
    benchmark_configure_thread(env, 0, seL4_MaxPrio-1, "broadcaster", &broadcaster);

    for (int run = 0; run < N_RUNS; ++run) {
        sel4utils_create_word_args(broadcast_args_strings, broadcast_argv, 
                N_BROADCAST_ARGS, ep, block_ep, lock, cv, &shared);

        for (int i = 0; i < N_WAITERS; ++i)  {
            sel4utils_create_word_args(wait_args_strings[i], wait_argv[i], N_WAIT_ARGS, ep, 
                    block_ep, lock, cv, &shared, (seL4_Word)(&(results->broadcast_wait_time[i][run])));
            error = sel4utils_start_thread(&waiters[i], waiter_func, 
                    (void *) N_WAIT_ARGS, (void *) wait_argv[i], 1);
        }
        assert(error == seL4_NoError);

        error = sel4utils_start_thread(&broadcaster, broadcast_func, 
                (void *) N_BROADCAST_ARGS, (void *) broadcast_argv, 1);
        assert(error == seL4_NoError);

        benchmark_wait_children(ep, "Broadcast bench waiters", N_WAITERS + 1);
    }

    for (int i = 0; i != N_WAITERS; ++i)  {
        sel4utils_clean_up_thread(&env->vka, &env->vspace, &waiters[i]);
    }
    sel4utils_clean_up_thread(&env->vka, &env->vspace, &broadcaster);
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    vka_object_t done_ep, block_ep;
    sync_results_t *results;
    sync_bin_sem_t lock;
    sync_cv_t cv;

    env = benchmark_get_env(argc, argv, sizeof(sync_results_t));
    results = (sync_results_t *) env->results;

    sel4bench_init();

    error = vka_alloc_endpoint(&env->vka, &done_ep);
    assert(error == seL4_NoError);

    error = vka_alloc_endpoint(&env->vka, &block_ep);
    assert(error == seL4_NoError);

    error = sync_bin_sem_new(&env->vka, &lock);
    assert(error == seL4_NoError);

    error = sync_cv_new(&env->vka, &cv);
    assert(error == seL4_NoError);

    benchmark_broadcast(env, done_ep.cptr, block_ep.cptr, &lock, &cv, results);

    vka_free_object(&env->vka, &done_ep);
    vka_free_object(&env->vka, &block_ep);

    sync_cv_destroy(&env->vka, &cv);
    sync_bin_sem_destroy(&env->vka, &lock);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

