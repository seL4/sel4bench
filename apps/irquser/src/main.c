/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <autoconf.h>
#include <sel4benchirquser/gen_config.h>
#include <stdio.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/irq.h>
#include <utils/time.h>

#include <platsupport/irq.h>
#include <platsupport/ltimer.h>

#include <sel4bench/arch/sel4bench.h>
#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>

#include <benchmark.h>
#include <irq.h>

#define INTERRUPT_PERIOD_NS (10 * NS_IN_MS)

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void spinner_fn(int argc, char **argv)
{
    sel4bench_init();
    if (argc != 1) {
        abort();
    }

    volatile ccnt_t *current_time = (volatile ccnt_t *) atol(argv[0]);

    while (1) {
        /* just take the low bits so the reads are atomic */
        SEL4BENCH_READ_CCNT(*current_time);
    }
}

/* ep for ticker to Send on when done */
static seL4_CPtr done_ep;
/* ntfn for ticker to wait for timer irqs on */
static seL4_CPtr timer_signal;
/* initialised IRQ interface */
static ps_irq_ops_t *irq_ops;
/* ntfn_id of the timer notification provided to the IRQ interface */
static ntfn_id_t timer_ntfn_id;

void ticker_fn(ccnt_t *results, volatile ccnt_t *current_time)
{
    seL4_Word start, end_low;
    ccnt_t end;
    seL4_Word badge;

    for (int i = 0; i < N_RUNS; i++) {
        /* wait for irq */
        seL4_Wait(timer_signal, &badge);
        /* record result */
        SEL4BENCH_READ_CCNT(end);
        sel4platsupport_irq_handle(irq_ops, timer_ntfn_id, badge);
        end_low = (seL4_Word) end;
        start = (seL4_Word) * current_time;
        results[i] = end_low - start;
    }

    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

void ticker_fn_ep(int argc, char **argv)
{
    if (argc != 5) {
        abort();
    }
    ccnt_t overhead = (ccnt_t) atol(argv[0]);
    ccnt_t *results_sum = (ccnt_t *) atol(argv[1]);
    ccnt_t *results_sum2 = (ccnt_t *) atol(argv[2]);
    ccnt_t *results_num = (ccnt_t *) atol(argv[3]);
    volatile ccnt_t *current_time = (volatile ccnt_t *) atol(argv[4]);

    seL4_Word start, end_low;
    ccnt_t end, sum = 0, sum2 = 0;
    seL4_Word badge;

    DATACOLLECT_INIT();

    for (seL4_Word i = 0; i < N_RUNS; i++) {
        /* wait for irq */
        seL4_Wait(timer_signal, &badge);
        /* record result */
        SEL4BENCH_READ_CCNT(end);
        sel4platsupport_irq_handle(irq_ops, timer_ntfn_id, badge);
        end_low = (seL4_Word) end;
        start = (seL4_Word) * current_time;
        DATACOLLECT_GET_SUMS(i, N_IGNORED, start, end_low, overhead, sum, sum2);
    }

    *results_sum = sum;
    *results_sum2 = sum2;
    *results_num = N_RUNS - N_IGNORED;

    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 2,
        [seL4_EndpointObject] = 1,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 2,
        [seL4_ReplyObject] = 2
#endif
    };

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(irquser_results_t),
              object_freq
          );
}

int main(int argc, char **argv)
{
    irquser_results_t *results;
    vka_object_t endpoint = {0};

    benchmark_init_timer(env);
    results = (irquser_results_t *) env->results;

    if (vka_alloc_endpoint(&env->slab_vka, &endpoint) != 0) {
        ZF_LOGF("Failed to allocate endpoint\n");
    }

    /* set up globals */
    done_ep = endpoint.cptr;
    timer_signal = env->ntfn.cptr;
    irq_ops = &env->io_ops.irq_ops;
    timer_ntfn_id = env->ntfn_id;

    int error = ltimer_reset(&env->ltimer);
    ZF_LOGF_IF(error, "Failed to start timer");

    error = ltimer_set_timeout(&env->ltimer, INTERRUPT_PERIOD_NS, TIMEOUT_PERIODIC);
    ZF_LOGF_IF(error, "Failed to configure timer");

    sel4bench_init();

    sel4utils_thread_t ticker, spinner;

    /* measurement overhead */
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results->overheads[i] = end - start;
    }

    /* find the minimum overhead for early processing run */
    results->overhead_min = getMinOverhead(results->overheads, N_RUNS);

    /* create a frame for the shared time variable so we can share it between processes */
    ccnt_t *local_current_time = (ccnt_t *) vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    if (local_current_time == NULL) {
        ZF_LOGF("Failed to allocate page");
    }

    /* first run the benchmark between two threads in the current address space */
    benchmark_configure_thread(env, endpoint.cptr, seL4_MaxPrio - 1, "ticker", &ticker);
    benchmark_configure_thread(env, endpoint.cptr, seL4_MaxPrio - 2, "spinner", &spinner);

    error = sel4utils_start_thread(&ticker, (sel4utils_thread_entry_fn) ticker_fn, (void *) results->thread_results,
                                   (void *) local_current_time, true);
    if (error) {
        ZF_LOGF("Failed to start ticker");
    }

    char strings[1][WORD_STRING_SIZE];
    char *spinner_argv[1];

    sel4utils_create_word_args(strings, spinner_argv, 1, (seL4_Word) local_current_time);
    error = sel4utils_start_thread(&spinner, (sel4utils_thread_entry_fn) spinner_fn, (void *) 1, (void *) spinner_argv,
                                   true);
    assert(!error);

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* stop spinner thread */
    error = seL4_TCB_Suspend(spinner.tcb.cptr);
    assert(error == seL4_NoError);

    error = seL4_TCB_Suspend(ticker.tcb.cptr);
    assert(error == seL4_NoError);

    /* run the benchmark again with early processing */
    char ticker_ep_strings[5][WORD_STRING_SIZE];
    char *ticker_ep_argv[5];
    sel4utils_create_word_args(ticker_ep_strings, ticker_ep_argv, 5, (seL4_Word) results->overhead_min,
                               &results->thread_results_ep_sum,
                               &results->thread_results_ep_sum2, &results->thread_results_ep_num, (seL4_Word) local_current_time);
    error = sel4utils_start_thread(&ticker, (sel4utils_thread_entry_fn) ticker_fn_ep, (void *) 5, (void *) ticker_ep_argv,
                                   true);
    if (error) {
        ZF_LOGF("Failed to start ticker");
    }

    error = sel4utils_start_thread(&spinner, (sel4utils_thread_entry_fn) spinner_fn, (void *) 1, (void *) spinner_argv,
                                   true);
    assert(!error);

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* stop spinner thread */
    error = seL4_TCB_Suspend(spinner.tcb.cptr);
    assert(error == seL4_NoError);

    error = seL4_TCB_Suspend(ticker.tcb.cptr);
    assert(error == seL4_NoError);

    /* now run the benchmark again, but run the spinner in another address space */

    /* restart ticker */
    error = sel4utils_start_thread(&ticker, (sel4utils_thread_entry_fn) ticker_fn, (void *) results->process_results,
                                   (void *) local_current_time, true);
    assert(!error);

    sel4utils_process_t spinner_process;
    benchmark_shallow_clone_process(env, &spinner_process, seL4_MaxPrio - 2, spinner_fn, "spinner");

    /* share the current time variable with the spinner process */
    void *current_time_remote = vspace_share_mem(&env->vspace, &spinner_process.vspace,
                                                 (void *) local_current_time, 1, seL4_PageBits,
                                                 seL4_AllRights, true);
    assert(current_time_remote != NULL);

    /* start the spinner process */
    sel4utils_create_word_args(strings, spinner_argv, 1, (seL4_Word) current_time_remote);
    error = benchmark_spawn_process(&spinner_process, &env->slab_vka, &env->vspace, 1, spinner_argv, 1);
    if (error) {
        ZF_LOGF("Failed to start spinner process");
    }

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* stop threads */
    error = seL4_TCB_Suspend(spinner_process.thread.tcb.cptr);
    assert(error == seL4_NoError);

    error = seL4_TCB_Suspend(ticker.tcb.cptr);
    assert(error == seL4_NoError);

    /* run the benchmark again but with early processing */
    sel4utils_create_word_args(ticker_ep_strings, ticker_ep_argv, 5, (seL4_Word) results->overhead_min,
                               &results->process_results_ep_sum,
                               &results->process_results_ep_sum2, &results->process_results_ep_num, (seL4_Word) local_current_time);
    error = sel4utils_start_thread(&ticker, (sel4utils_thread_entry_fn) ticker_fn_ep, (void *) 5, (void *) ticker_ep_argv,
                                   true);
    assert(!error);

    /* start the spinner process */
    sel4utils_create_word_args(strings, spinner_argv, 1, (seL4_Word) current_time_remote);
    error = benchmark_spawn_process(&spinner_process, &env->slab_vka, &env->vspace, 1, spinner_argv, 1);
    if (error) {
        ZF_LOGF("Failed to start spinner process");
    }

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
