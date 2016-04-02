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

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <utils/time.h>

#include <sel4bench/arch/sel4bench.h>
#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>

#include <benchmark.h>
#include <irq.h>

#define INTERRUPT_PERIOD_NS (100 * NS_IN_MS)

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
spinner_fn(int argc, char **argv)
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
/* timer */
seL4_timer_t *timer;

void
ticker_fn(ccnt_t *results, volatile ccnt_t *current_time)
{
    seL4_Word start, end_low;
    ccnt_t end;

    sel4_timer_handle_single_irq(timer);
    seL4_Wait(timer_signal, NULL);
    for (int i = 0; i < N_RUNS; i++) {
        sel4_timer_handle_single_irq(timer);
        /* wait for irq */
        seL4_Wait(timer_signal, NULL);
        /* record result */
        SEL4BENCH_READ_CCNT(end);
        end_low = (seL4_Word) end;
        start = (seL4_Word) *current_time;
        results[i] = end_low - start;
    }

    seL4_Signal(done_ep);
}

int
main(int argc, char **argv)
{
    env_t *env;
    int error;
    irquser_results_t *results;
    vka_object_t notification = {0};
    vka_object_t endpoint = {0};

    env = benchmark_get_env(argc, argv, sizeof(irquser_results_t));
    results = (irquser_results_t *) env->results;

    if (vka_alloc_endpoint(&env->vka, &endpoint) != 0) {
        ZF_LOGF("Failed to allocate endpoint\n");
    }
    done_ep = endpoint.cptr;

    /* Set up timer as a source of interrupts */
    if (vka_alloc_notification(&env->vka, &notification) != 0) {
        ZF_LOGF("Failed to allocate notification\n");
    }
    timer_signal = notification.cptr;

    timer = sel4platsupport_get_default_timer(&env->vka, &env->vspace, &env->simple,
                                              notification.cptr);
    if (timer == NULL) {
        ZF_LOGF("Failed to access timer driver\n");
    }

    if (timer_start(timer->timer) != 0) {
        ZF_LOGF("Failed to start timer\n");
    }

    if (timer_periodic(timer->timer, INTERRUPT_PERIOD_NS) != 0) {
        ZF_LOGF("Failed to configure timer\n");
    }

    sel4bench_init();

    sel4utils_thread_t ticker, spinner;

    /* measurement overhead */
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results->overheads[i] = end - start;
    }

    /* create a frame for the shared time variable so we can share it between processes */
    ccnt_t *local_current_time = (ccnt_t *) vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    if (local_current_time == NULL) {
        ZF_LOGF("Failed to allocate page");
    }

    /* first run the benchmark between two threads in the current address space */
    benchmark_configure_thread(env, endpoint.cptr, seL4_MaxPrio - 1, "ticker", &ticker);
    benchmark_configure_thread(env, endpoint.cptr, seL4_MaxPrio - 2, "spinner", &spinner);
    
    error = sel4utils_start_thread(&ticker, ticker_fn, (void *) results->thread_results, 
                                   (void *) local_current_time, true);
    if (error) {
        ZF_LOGF("Failed to start ticker");
    }

    char strings[1][WORD_STRING_SIZE];
    char *spinner_argv[1];

    sel4utils_create_word_args(strings, spinner_argv, 1, (seL4_Word) local_current_time);
    error = sel4utils_start_thread(&spinner, spinner_fn, (void *) 1, (void *) spinner_argv, true);
    assert(!error);

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* stop spinner thread */
    error = seL4_TCB_Suspend(spinner.tcb.cptr);
    assert(error == seL4_NoError);
    
    error = seL4_TCB_Suspend(ticker.tcb.cptr);
    assert(error == seL4_NoError);
    
    /* now run the benchmark again, but run the spinner in another address space */
    
    /* restart ticker */
    error = sel4utils_start_thread(&ticker, ticker_fn, (void *) results->process_results, 
                                   (void *) local_current_time, true);
    assert(!error);

    sel4utils_process_t spinner_process;
    benchmark_shallow_clone_process(env, &spinner_process, seL4_MaxPrio - 2, spinner_fn, "spinner");

    /* share the current time variable with the spinner process */
    void *current_time_remote = vspace_share_mem(&env->vspace, &spinner_process.vspace,
                                                 (void *) local_current_time, 2, seL4_PageBits, 
                                                 seL4_AllRights, true);
    assert(current_time_remote != NULL);

    /* start the spinner process */
    sel4utils_create_word_args(strings, spinner_argv, 1, (seL4_Word) current_time_remote);
    error = sel4utils_spawn_process(&spinner_process, &env->vka, &env->vspace, 1, spinner_argv, 1);
    if (error) {
        ZF_LOGF("Failed to start spinner process");
    }

    benchmark_wait_children(endpoint.cptr, "child of irq-user", 1);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

