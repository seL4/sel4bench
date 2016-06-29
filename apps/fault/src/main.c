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
#include <utils/ud.h>

#include <benchmark.h>
#include <fault.h>

#define NOPS ""
#include <arch/fault.h>

#define N_FAULTER_ARGS 3
#define N_HANDLER_ARGS 4

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

static inline void
fault(void)
{
    utils_undefined_instruction();
}

static void
parse_handler_args(int argc, char **argv,
                   seL4_CPtr *ep, volatile ccnt_t **start, fault_results_t **results,
                   seL4_CPtr *done_ep)
{
    assert(argc == N_HANDLER_ARGS);
    *ep = atol(argv[0]);
    *start = (volatile ccnt_t *) atol(argv[1]);
    *results = (fault_results_t *) atol(argv[2]);
    *done_ep = atol(argv[3]);
}

static inline void
fault_handler_done(seL4_CPtr ep, seL4_Word ip, seL4_CPtr done_ep)
{
    /* handle last fault */
    ip += UD_INSTRUCTION_SIZE;
    seL4_ReplyWith1MR(ip);
    /* tell benchmark we are done */
    seL4_Signal(done_ep);
    /* block */
    seL4_Recv(ep, NULL);
}

static inline seL4_Word
fault_handler_start(seL4_CPtr ep)
{
    seL4_Word ip;
    /* wait for first fault */
    seL4_RecvWith1MR(ep, &ip);
    return ip;
}

/* Pair for measuring fault -> fault handler path */
static void
measure_fault_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    volatile ccnt_t * start = (volatile ccnt_t *) atol(argv[0]);
    seL4_CPtr done_ep = atol(argv[2]);

    for (int i = 0; i < N_RUNS + 1; i++) {
        /* record time */
        SEL4BENCH_READ_CCNT(*start);
        fault();
    }
    seL4_Signal(done_ep);
}

static void
measure_fault_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep;
    volatile ccnt_t *start;
    ccnt_t end;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep);

    seL4_Word ip = fault_handler_start(ep);
    for (int i = 0; i < N_RUNS; i++) {
        ip += UD_INSTRUCTION_SIZE;
        DO_REAL_REPLY_RECV_1(ep, ip);

        SEL4BENCH_READ_CCNT(end);
        results->fault[i] = end - *start;
    }
    fault_handler_done(ep, ip, done_ep);
}

/* Pair for measuring fault handler -> faultee path */
static void
measure_fault_reply_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[0]);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);

    /* handle 1 fault first to make sure start is set */
    fault();
    for (int i = 0; i < N_RUNS + 1; i++) {
        fault();
        ccnt_t end;
        SEL4BENCH_READ_CCNT(end);
        results->fault_reply[i] = end - *start;
    }
    seL4_Signal(done_ep);
}

static void
measure_fault_reply_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep;
    volatile ccnt_t *start;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep);

    seL4_Word ip = fault_handler_start(ep);
    for (int i = 0; i <= N_RUNS; i++) {
        ip += UD_INSTRUCTION_SIZE;
        /* record time */
        SEL4BENCH_READ_CCNT(*start);
        /* wait for fault */
        DO_REAL_REPLY_RECV_1(ep, ip);
    }
    fault_handler_done(ep, ip, done_ep);
}

/* round_trip fault handling pair */
static void
measure_fault_roundtrip_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);

    for (int i = 0; i < N_RUNS + 1; i++) {
        ccnt_t start, end;
        SEL4BENCH_READ_CCNT(start);
        fault();
        SEL4BENCH_READ_CCNT(end);
        results->round_trip[i] = end - start;
    }
    seL4_Signal(done_ep);
}

static void
measure_fault_roundtrip_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep;
    UNUSED volatile ccnt_t *start;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep);

    seL4_Word ip = fault_handler_start(ep);
    for (int i = 0; i < N_RUNS; i++) {
        /* wait for fault */
        ip += UD_INSTRUCTION_SIZE;
        DO_REAL_REPLY_RECV_1(ep, ip);
    }
    fault_handler_done(ep, ip, done_ep);
}

static void
run_fault_benchmark(env_t *env, fault_results_t *results)
{
    /* allocate endpoint */
    vka_object_t fault_endpoint = {0};
    UNUSED int error = vka_alloc_endpoint(&env->vka, &fault_endpoint);
    assert(error == 0);

    vka_object_t done_ep = {0};
    error = vka_alloc_endpoint(&env->vka, &done_ep);
    assert(error == 0);

    /* create faulter */
    ccnt_t start = 0;
    char faulter_args[N_FAULTER_ARGS][WORD_STRING_SIZE];
    char *faulter_argv[N_FAULTER_ARGS];
    sel4utils_thread_t faulter;

    sel4utils_create_word_args(faulter_args, faulter_argv, N_FAULTER_ARGS, (seL4_Word) &start,
                               (seL4_Word) results, done_ep.cptr);
    benchmark_configure_thread(env, fault_endpoint.cptr, seL4_MinPrio + 1, "faulter", &faulter);

    /* create fault handler */
    sel4utils_thread_t fault_handler;
    char handler_args[N_HANDLER_ARGS][WORD_STRING_SIZE];
    char *handler_argv[N_HANDLER_ARGS];
    sel4utils_create_word_args(handler_args, handler_argv, N_HANDLER_ARGS,
                               fault_endpoint.cptr, (seL4_Word) &start,
                               (seL4_Word) results, done_ep.cptr);
    benchmark_configure_thread(env, seL4_CapNull, seL4_MinPrio, "fault handler", &fault_handler);

    /* benchmark fault */
    error = sel4utils_start_thread(&faulter, measure_fault_fn,
                                   (void *) N_FAULTER_ARGS, (void *) faulter_argv, true);
    assert(error == 0);
    error = sel4utils_start_thread(&fault_handler, measure_fault_handler_fn,
                                   (void *) N_HANDLER_ARGS, (void *) handler_argv, true);
    assert(error == 0);
    benchmark_wait_children(done_ep.cptr, "fault handler", 2);

    /* benchmark reply */
    error = sel4utils_start_thread(&faulter, measure_fault_reply_fn,
                                   (void *) N_FAULTER_ARGS, (void *) faulter_argv, true);
    assert(error == 0);
    error = sel4utils_start_thread(&fault_handler, measure_fault_reply_handler_fn,
                                   (void *) N_HANDLER_ARGS, (void *) handler_argv, true);
    assert(error == 0);
    benchmark_wait_children(done_ep.cptr, "fault handler", 2);

    /* benchmark round_trip */
    error = sel4utils_start_thread(&faulter, measure_fault_roundtrip_fn,
                                   (void *) N_FAULTER_ARGS, (void *) faulter_argv, true);
    assert(error == 0);
    error = sel4utils_start_thread(&fault_handler, measure_fault_roundtrip_handler_fn,
                                   (void *) N_HANDLER_ARGS, (void *) handler_argv, true);
    assert(error == 0);
    benchmark_wait_children(done_ep.cptr, "fault handler", 2);
}

void
measure_overhead(fault_results_t *results)
{
    ccnt_t start, end;
    seL4_CPtr ep = 0;
    UNUSED seL4_Word mr0 = 0;

    /* overhead of reply recv stub + cycle count */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_REPLY_RECV_1(ep, mr0);
        SEL4BENCH_READ_CCNT(end);
        results->reply_recv_overhead[i] = (end - start);
    }

    /* overhead of cycle count */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results->ccnt_overhead[i] = (end - start);
    }
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    fault_results_t *results;

    env = benchmark_get_env(argc, argv, sizeof(fault_results_t));
    results = (fault_results_t *) env->results;

    sel4bench_init();

    measure_overhead(results);
    run_fault_benchmark(env, results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

