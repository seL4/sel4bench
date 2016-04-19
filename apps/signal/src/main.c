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

#include <benchmark.h>
#include <signal.h>

#define NOPS ""
#define __SWINUM(x) ((x) & 0x00ffffff)

#include <arch/signal.h>
#define N_SIGNAL_ARGS 4
#define N_WAIT_ARGS 3

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
wait_fn(int argc, char **argv) {

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

void 
signal_fn(int argc, char **argv)
{
    assert(argc == N_SIGNAL_ARGS);
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

static void
benchmark(env_t *env, seL4_CPtr ep, seL4_CPtr ntfn, ccnt_t *results)
{
    sel4utils_thread_t wait, signal;
    char wait_args_strings[N_WAIT_ARGS][WORD_STRING_SIZE];
    char *wait_argv[N_WAIT_ARGS];
    char signal_args_strings[N_SIGNAL_ARGS][WORD_STRING_SIZE];
    char *signal_argv[N_SIGNAL_ARGS];
    ccnt_t end;
    UNUSED int error;

    benchmark_configure_thread(env, ep, seL4_MaxPrio, "wait", &wait);
    benchmark_configure_thread(env, ep, seL4_MaxPrio - 1, "signal", &signal);
     
    sel4utils_create_word_args(wait_args_strings, wait_argv, N_WAIT_ARGS, ntfn, 
                               ep, (seL4_Word) &end);
    sel4utils_create_word_args(signal_args_strings, signal_argv, N_SIGNAL_ARGS, ntfn, 
                               (seL4_Word) &end, (seL4_Word) results, ep);

    printf("Starting thread\n");
    error = sel4utils_start_thread(&signal, signal_fn, (void *) N_SIGNAL_ARGS, (void *) signal_argv, 1);
    assert(error == seL4_NoError);
    error = sel4utils_start_thread(&wait, wait_fn, (void *) N_WAIT_ARGS, (void *) wait_argv, 1);
    assert(error == seL4_NoError);

    benchmark_wait_children(ep, "children of notification benchmark", 2);
        
    sel4utils_clean_up_thread(&env->vka, &env->vspace, &wait);
    sel4utils_clean_up_thread(&env->vka, &env->vspace, &signal);
}   

void
measure_signal_overhead(seL4_CPtr ntfn, ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_SIGNAL(ntfn);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    vka_object_t done_ep, ntfn;
    signal_results_t *results;

    env = benchmark_get_env(argc, argv, sizeof(signal_results_t));
    results = (signal_results_t *) env->results;

    sel4bench_init();

    error = vka_alloc_endpoint(&env->vka, &done_ep);
    assert(error == seL4_NoError);
    
    error = vka_alloc_notification(&env->vka, &ntfn);
    assert(error == seL4_NoError);

    /* measure overhead */    
    measure_signal_overhead(ntfn.cptr, results->overhead);
        
    benchmark(env, done_ep.cptr, ntfn.cptr, results->results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

