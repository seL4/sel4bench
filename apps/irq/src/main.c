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
#include <sel4benchirq/gen_config.h>

#include <stdio.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <utils/time.h>

#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>

#include <benchmark.h>
#include <irq.h>

#define N_INTERRUPTS 500
#define INTERRUPT_PERIOD_NS (100 * NS_IN_MS)

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    env_t *env;
    irq_results_t *results;

    static size_t object_freq[seL4_ObjectTypeCount] = {0};
    env = benchmark_get_env(argc, argv, sizeof(irq_results_t), object_freq);
    results = (irq_results_t *) env->results;

    ZF_LOGF_IF(timer_periodic(env->timeout_timer->timer, INTERRUPT_PERIOD_NS) != 0,
               "Failed to configure timer\n");

    ZF_LOGF_IF(timer_start(env->timeout_timer->timer) != 0, "Failed to start timer\n");

    sel4bench_init();

    /* Record tracepoints for irq path */
    kernel_logging_reset_log();
    for (int i = 0; i < N_INTERRUPTS; ++i) {
        seL4_Recv(env->ntfn.cptr, NULL);
        /* Each time an interrupt is processed by the kernel,
         * several tracepoints are invoked in the kernel which record
         * cycle counts between pairs of points.
         *
         * TRACE_POINT_OVERHEAD counts the cycles consumed by starting
         * and immediately stopping a tracepoint.
         * TRACE_POINT_IRQ_PATH_START and TRACE_POINT_IRQ_PATH_END each
         * count cycles for part of the IRQ path (from the point where
         * an interrupt is received by the kernel to the return to user
         * mode). Multiple tracepoints are used to record cycle counts
         * only when a specific path between these two points is followed.
         */
        sel4_timer_handle_single_irq(env->timeout_timer);
    }
    kernel_logging_finalize_log();

    sel4bench_destroy();

    ZF_LOGF_IF(timer_stop(env->timeout_timer->timer) != 0, "Failed to stop timer\n");

    /* Extract data from kernel */
    results->n = kernel_logging_sync_log(results->kernel_log, KERNEL_MAX_NUM_LOG_ENTRIES);

    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
