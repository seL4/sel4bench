/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <sync.h>
#include <stdio.h>

static json_t *sync_process(void *results)
{
    sync_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = false,
        .name = "sync benchmarks",
        .ignored = N_IGNORED
    };

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

    result_t wait_results[N_WAITERS];

    result_set_t set = {
        .name = "",
        .extra_cols = &extra,
        .n_extra_cols = 1,
        .results = wait_results,
        .n_results = N_WAITERS,
    };

    json_t *array = json_array();

    for (int j = 0; j < N_BROADCAST_BENCHMARKS; ++j) {
        process_results(N_WAITERS, N_RUNS, raw_results->broadcast_wait_time[j], desc, wait_results);
        set.name = broadcast_wait_names[j];
        json_array_append_new(array, result_set_to_json(set));
    }

    result_t result;
    set.n_extra_cols = 0;
    set.extra_cols = NULL;
    set.results = &result;
    set.n_results = 1;

    for (int j = 0; j < N_BROADCAST_BENCHMARKS; ++j) {
        result = process_result(N_RUNS, raw_results->broadcast_broadcast_time[j], desc);
        set.name = broadcast_broadcast_names[j];
        json_array_append_new(array, result_set_to_json(set));
    }

    for (int j = 0; j < N_PROD_CONS_BENCHMARKS; ++j) {
        result = process_result(N_RUNS, raw_results->producer_to_consumer[j], desc);
        set.name = producer_to_consumer_names[j];
        json_array_append_new(array, result_set_to_json(set));
    }

    for (int j = 0; j < N_PROD_CONS_BENCHMARKS; ++j) {
        result = process_result(N_RUNS, raw_results->consumer_to_producer[j], desc);
        set.name = consumer_to_producer_names[j];
        json_array_append_new(array, result_set_to_json(set));
    }

    result = process_result_early_proc(raw_results->producer_to_consumer_ep_num,
                                       raw_results->producer_to_consumer_ep_sum,
                                       raw_results->producer_to_consumer_ep_sum2,
                                       raw_results->producer_to_consumer_ep);
    set.name = "Producer to consumer (early processing)";
    json_array_append_new(array, result_set_to_json(set));

    result = process_result_early_proc(raw_results->consumer_to_producer_ep_num,
                                       raw_results->consumer_to_producer_ep_sum,
                                       raw_results->consumer_to_producer_ep_sum2,
                                       raw_results->consumer_to_producer_ep);
    set.name = "Consumer to producer (early processing)";
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

benchmark_t *sync_benchmark_new(void)
{
    return &sync_benchmark;
}
