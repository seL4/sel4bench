/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <scheduler.h>
#include <stdio.h>

static void process_yield_results(scheduler_results_t *results, ccnt_t overhead, json_t *array)
{
    result_desc_t desc = {
        .ignored = N_IGNORED,
        .overhead = overhead,
    };

    result_t result;
    result_set_t set = {
        .name = "Thread yield",
        .n_extra_cols = 0,
        .results = &result,
        .n_results = 1,
    };

    result = process_result(N_RUNS, results->thread_yield, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Thread yield (early processing)";
    result = process_result_early_proc(results->thread_yield_ep_num, results->thread_yield_ep_sum,
                                       results->thread_yield_ep_sum2, results->thread_yield_ep);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Process yield";
    result = process_result(N_RUNS, results->process_yield, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Process yield (early processing)";
    result = process_result_early_proc(results->process_yield_ep_num, results->process_yield_ep_sum,
                                       results->process_yield_ep_sum2, results->process_yield_ep);
    json_array_append_new(array, result_set_to_json(set));

    result_t average_results[NUM_AVERAGE_EVENTS];
    process_average_results(N_RUNS, NUM_AVERAGE_EVENTS, results->average_yield, average_results);
    json_array_append_new(array, average_counters_to_json("Average seL4_Yield (no thread switch)",
                                                          average_results));
}

static void process_scheduler_results(scheduler_results_t *results, json_t *array)
{
    result_desc_t desc = {
        .stable = true,
        .name = "Signal overhead",
        .ignored = N_IGNORED
    };
    result_t result = process_result(N_RUNS, results->overhead_signal, desc);
    result_t per_prio_result[N_PRIOS];

    /* signal overhead */
    result_set_t set = {
        .name = "Signal overhead",
        .n_extra_cols = 0,
        .results = &result,
        .n_results = 1,
    };
    json_array_append_new(array, result_set_to_json(set));

    /* thread switch overhead */
    desc.stable = false;
    desc.overhead = result.min;

    process_results(N_PRIOS, N_RUNS, results->thread_results, desc, per_prio_result);

    /* construct prio column */
    json_int_t column_values[N_PRIOS];
    for (json_int_t i = 0; i < N_PRIOS; i++) {
        /* generate the prios corresponding to the benchmarked prio values */
        column_values[i] = gen_next_prio(i);
    }

    column_t extra = {
        .header = "Prio",
        .type = JSON_INTEGER,
        .integer_array = &column_values[0]
    };

    set.name = "Signal to thread of higher prio";
    set.extra_cols = &extra,
    set.n_extra_cols = 1,
    set.results = per_prio_result,
    set.n_results = N_PRIOS,
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Signal to process of higher prio";
    process_results(N_PRIOS, N_RUNS, results->process_results, desc, per_prio_result);
    json_array_append_new(array, result_set_to_json(set));

    result_t average_results[NUM_AVERAGE_EVENTS];
    process_average_results(N_RUNS, NUM_AVERAGE_EVENTS, results->set_prio_average, average_results);
    json_array_append_new(array, average_counters_to_json("Average to reschedule current thread",
                                                          average_results));
}

static json_t *scheduler_process(void *results)
{
    scheduler_results_t *raw_results = results;
    json_t *array = json_array();

    process_scheduler_results(raw_results, array);

    result_desc_t desc = {
        .name = "Read ccnt overhead",
        .stable = true,
        .ignored = N_IGNORED,
    };

    result_t ccnt_overhead = process_result(N_RUNS, raw_results->overhead_ccnt, desc);

    result_set_t set = {
        .name = "Read ccnt overhead",
        .n_extra_cols = 0,
        .results = &ccnt_overhead,
        .n_results = 1
    };

    json_array_append_new(array, result_set_to_json(set));

    process_yield_results(raw_results, ccnt_overhead.min, array);

    return array;
}

static benchmark_t sched_benchmark = {
    .name = "scheduler",
    .enabled = config_set(CONFIG_APP_SCHEDULERBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(scheduler_results_t), seL4_PageBits),
    .process = scheduler_process,
    .init = blank_init
};

benchmark_t *scheduler_benchmark_new(void)
{
    return &sched_benchmark;
}
