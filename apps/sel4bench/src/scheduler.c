/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <scheduler.h>
#include <stdio.h>

static json_t *
scheduler_process(void *results) {
    scheduler_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = true,
        .name = "Signal overhead",
        .ignored = N_IGNORED
    };

    result_t result = process_result(N_RUNS, raw_results->overhead_signal, desc);
    result_t per_prio_result[N_PRIOS];

    /* signal overhead */
    json_t *array = json_array();
    result_set_t set = {
        .name = "Signal overhead",
        .n_extra_cols = 0,
        .results = &result,
        .n_results = 1,
        .samples = N_RUNS - N_IGNORED
    };
    json_array_append_new(array, result_set_to_json(set));

    /* thread switch overhead */
    desc.stable = false;
    desc.overhead = result.min;

    process_results(N_PRIOS, N_RUNS, raw_results->thread_results, desc, per_prio_result);

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
    set.samples = N_RUNS - N_IGNORED;

    json_array_append_new(array, result_set_to_json(set));

    set.name = "Signal to process of higher prio";
    process_results(N_PRIOS, N_RUNS, raw_results->process_results, desc, per_prio_result);
    json_array_append_new(array, result_set_to_json(set));

    desc.name = "Yield overhead";
    desc.stable = true;

    result = process_result(N_RUNS, raw_results->overhead_yield, desc);

    set.name = "Yield overhead",
    set.n_extra_cols = 0,
    set.results = &result,
    set.n_results = 1,

    json_array_append_new(array, result_set_to_json(set));

    desc.stable = false;
    desc.overhead = result.min;

    result = process_result(N_RUNS, raw_results->thread_yield, desc);
    set.name = "Thread yield";
    json_array_append_new(array, result_set_to_json(set));

    result = process_result(N_RUNS, raw_results->process_yield, desc);
    set.name = "Process yield";
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t sched_benchmark = {
    .name = "scheduler",
    .enabled = config_set(CONFIG_APP_SCHEDULERBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(scheduler_results_t), seL4_PageBits),
    .process = scheduler_process,
    .init = blank_init
};

benchmark_t *
scheduler_benchmark_new(void) 
{
    return &sched_benchmark;
}

