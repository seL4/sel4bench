/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>

#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <trace_clearMemory.h>
#include <sel4platsupport/device.h>
#include <stdio.h>
#include <utils/time.h>

#define TRACE_POINT_OVERHEAD 0
#define TRACE_POINT_CLEAR_MEMORY 1

#ifndef CONFIG_MAX_NUM_TRACE_POINTS
#define CONFIG_MAX_NUM_TRACE_POINTS 0
#endif

static ccnt_t kernel_log_data[KERNEL_MAX_NUM_LOG_ENTRIES];
static unsigned int offsets[CONFIG_MAX_NUM_TRACE_POINTS];
static unsigned int sizes[CONFIG_MAX_NUM_TRACE_POINTS];

static json_t *process(void *results)
{
    trace_clear_memory_results_t *trace_results = (trace_clear_memory_results_t *) results;

    /* Sort and group data by tracepoints. A stable sort is used so the first N_IGNORED
     * results of each tracepoint can be ignored, as this keeps the data in chronological
     * order.
     */
    logging_stable_sort_log(trace_results->kernel_log, trace_results->n);
    logging_group_log_by_key(trace_results->kernel_log, trace_results->n, sizes, offsets, CONFIG_MAX_NUM_TRACE_POINTS);

    /* Copy the cycle counts into a separate array to simplify further processing */
    for (int i = 0; i < trace_results->n; ++i) {
        kernel_log_data[i] = kernel_logging_entry_get_data(&trace_results->kernel_log[i]);
    }

    /* Process log entries generated by an "empty" tracepoint, which recorded
     * the number of cycles between starting a tracepoint and stopping it
     * immediately afterwards. This will determine the overhead introduced by
     * using tracepoints.
     */
    int n_overhead_data = sizes[TRACE_POINT_OVERHEAD] - N_IGNORED;
    if (n_overhead_data <= 0) {
        ZF_LOGF("Insufficient data recorded. Was the kernel built with the relevant tracepoints?\n");
    }

    ccnt_t *overhead_data = &kernel_log_data[offsets[TRACE_POINT_OVERHEAD] + N_IGNORED];

    /* A new buffer is allocated to store the processed results. */
    int n_data = sizes[TRACE_POINT_CLEAR_MEMORY] - N_IGNORED;
    if (n_data <= 0) {
        ZF_LOGF("Insufficient data recorded. Was the kernel built with the relevant tracepoints?\n");
    }

    ccnt_t *data = (ccnt_t *)malloc(sizeof(ccnt_t) * n_data);
    if (data == NULL) {
        ZF_LOGF("Failed to allocate memory\n");
    }

    json_t *array = json_array();
    result_desc_t desc = {0};
    result_t result = process_result(n_overhead_data, overhead_data, desc);

    result_set_t set = {
        .name = "Tracepoint overhead",
        .n_results = 1,
        .results = &result,
    };

    json_array_append_new(array, result_set_to_json(set));

    /* Process the data by subtracting the overhead off the measured result. */
    ccnt_t *starts = &kernel_log_data[offsets[TRACE_POINT_CLEAR_MEMORY] + N_IGNORED];
    for (int i = 0; i < n_data; ++i) {
        data[i] = starts[i] - result.mean;
    }

#define STRINGIZE_(var) #var
#define STRINGIZE(var) STRINGIZE_(var)
    set.name = "clearMemory(chunksize = " STRINGIZE(CONFIG_RESET_CHUNK_BITS) " bits) Cycle Count (accounting for overhead)";

    result = process_result(n_data, data, desc);
    json_array_append_new(array, result_set_to_json(set));

    free(data);

    return array;
}

static benchmark_t trace_clearMemory_benchmark = {
    .name = "traceclearmemory",
    .enabled = config_set(CONFIG_APP_TRACE_CLEARMEMORY_BENCH) && CONFIG_MAX_NUM_TRACE_POINTS == 2,
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(trace_clear_memory_results_t), seL4_PageBits),
    .process = process,
    .init = blank_init
};

benchmark_t *trace_clearMemory_benchmark_new(void)
{
    return &trace_clearMemory_benchmark;
}
