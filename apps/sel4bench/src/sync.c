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
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <sync.h>
#include <stdio.h>

static json_t *
sync_process(void *results) {
    sync_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = true,
        .name = "signal overhead",
        .ignored = N_IGNORED
    };

    result_t result = process_result(N_RUNS, raw_results->overhead, desc);

    result_set_t set = {
        .name = "Signal overhead",
        .n_results = 1,
        .n_extra_cols = 0,
        .results = &result
    };

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(set));

    desc.stable = false;
    desc.overhead = result.min;

    result = process_result(N_RUNS, raw_results->lo_prio_results, desc);
    set.name = "Signal to high prio thread";
    json_array_append_new(array, result_set_to_json(set));

    result = process_result(N_RUNS, raw_results->hi_prio_results, desc);
    set.name = "Signal to low prio thread";
    json_array_append_new(array, result_set_to_json(set));

    result_t wait_results[N_WAITERS];
    process_results(N_WAITERS, N_RUNS, raw_results->broadcast_wait_time, desc, wait_results);

    /* construct waiter column */
    json_int_t column_values[N_WAITERS];
    for (json_int_t i = 0; i < N_WAITERS; i++) {
        column_values[i] = i;
    }

    column_t extra = {
        .header = "Waiter",
        .type = JSON_INTEGER,
        .integer_array = &column_values[0]
    };

    set.name = "Broadcast wait time";
    set.extra_cols = &extra,
    set.n_extra_cols = 1,
    set.results = wait_results,
    set.n_results = N_WAITERS,
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t sync_benchmark = {
    .name = "sync",
    .enabled = config_set(CONFIG_APP_SYNCBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(sync_results_t), seL4_PageBits),
    .process = sync_process,
    .init = blank_init
};

benchmark_t *
sync_benchmark_new(void) 
{
    return &sync_benchmark;
}

