/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include "benchmark.h"
#include "processing.h"
#include "timing.h"

#include <stdio.h>

#include <platsupport/mach/epit.h>
#include <sel4platsupport/mach/epit.h>

#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>

#define N_INTERRUPTS 500
#define N_IGNORED 10

#define TRACE_POINT_OVERHEAD 0
#define TRACE_POINT_IRQ_PATH_START 1
#define TRACE_POINT_IRQ_PATH_END 2

log_entry_t kernel_log[KERNEL_LOG_BUFFER_SIZE];
seL4_Word kernel_log_data[KERNEL_LOG_BUFFER_SIZE];
unsigned int offsets[CONFIG_MAX_NUM_TRACE_POINTS];
unsigned int sizes[CONFIG_MAX_NUM_TRACE_POINTS];

static void print_result(bench_result_t *result) {
    printf("min\tmax\tmean\tvariance\tstddev\tstddev %%\n");

    printf(CCNT_FORMAT"\t", result->min);
    printf(CCNT_FORMAT"\t", result->max);
    printf("%.2lf\t", result->mean);
    printf("%.2lf\t", result->variance);
    printf("%.2lf\t", result->stddev);
    printf("%.0lf%%\n", result->stddev_pc);
}

void
irq_benchmarks_new(struct env* env) {

    vka_object_t timer_aep_object;

    /* Set up timer as a source of interurpts */
    vka_alloc_async_endpoint(&env->vka, &timer_aep_object);
    seL4_CPtr timer_aep = timer_aep_object.cptr;

    seL4_timer_t *timer = sel4platsupport_get_epit(&env->vspace, &env->simple, &env->vka, timer_aep, 0, EPIT1);

    /* Interrupt every 10 ms */
    timer_periodic(timer->timer, 10000000);
    timer_start(timer->timer);

    timing_init();

    /* Record tracepoints for irq path */
    kernel_logging_reset_log();
    for (int i = 0; i < N_INTERRUPTS; ++i) {
        seL4_Wait(timer_aep, NULL);
        sel4_timer_handle_irq(timer, EPIT1_INTERRUPT);
    }
    kernel_logging_finalize_log();

    timing_destroy();

    /* Extract data from kernel */
    unsigned int n = kernel_logging_sync_log(kernel_log, KERNEL_LOG_BUFFER_SIZE);

    /* Sort and group data by tracepoints. A stable sort is used so the first N_IGNORED
     * results of each tracepoint can be ignored, as this keeps the data in cronological
     * order.
     */
    logging_sort_log(kernel_log, n);
    logging_group_log_by_key(kernel_log, n, sizes, offsets, CONFIG_MAX_NUM_TRACE_POINTS);

    /* Copy the cycle counts into a separate array to simplify further processing */
    for (int i = 0; i < n; ++i) {
        kernel_log_data[i] = kernel_log[i].data;
    }

    int n_overhead_data = sizes[TRACE_POINT_OVERHEAD] - N_IGNORED;
    seL4_Word *overhead_data = &kernel_log_data[offsets[TRACE_POINT_OVERHEAD] + N_IGNORED];
    bench_result_t overhead_result = process_result(overhead_data, n_overhead_data, NULL);

    int n_data = sizes[TRACE_POINT_IRQ_PATH_START] - N_IGNORED;
    seL4_Word *data = (seL4_Word*)malloc(sizeof(seL4_Word) * n_data);
    if (data == NULL) {
    }
    seL4_Word *starts = &kernel_log_data[offsets[TRACE_POINT_IRQ_PATH_START] + N_IGNORED];
    seL4_Word *ends = &kernel_log_data[offsets[TRACE_POINT_IRQ_PATH_END] + N_IGNORED];
    for (int i = 0; i < n_data; ++i) {
        data[i] = starts[i] + ends[i] - (overhead_result.mean * 2);
    }

    printf("Ignoring first %d records.\n\n", N_IGNORED);

    printf("----------------------------------------\n");
    printf("Tracepoint Overhead (%d samples)\n", n_overhead_data);
    printf("----------------------------------------\n");
    bench_result_t result = process_result(overhead_data, n_overhead_data, NULL);
    print_result(&result);
    printf("\n");

    printf("----------------------------------------\n");
    printf("IRQ Path Cycle Count (accountnig for overhead) (%d samples)\n", n_data);
    printf("----------------------------------------\n");
    result = process_result(data, n_data, NULL);
    print_result(&result);
    printf("\n");
}
