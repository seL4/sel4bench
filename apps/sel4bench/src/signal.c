/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <sel4benchsupport/signal.h>
#include <stdio.h>

static json_t *
signal_process(void *results) {
    signal_results_t *raw_results = results;

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

    result_t average_results[NUM_AVERAGE_EVENTS];
    process_average_results(N_RUNS, NUM_AVERAGE_EVENTS, raw_results->hi_prio_average, average_results);

    json_array_append_new(array, average_counters_to_json("Average signal to low prio thread",
                                                           average_results));

    return array;
}

static benchmark_t signal_benchmark = {
    .name = "signal",
    .enabled = config_set(CONFIG_APP_SIGNALBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(signal_results_t), seL4_PageBits),
    .process = signal_process,
    .init = blank_init
};

benchmark_t *
signal_benchmark_new(void)
{
    return &signal_benchmark;
}
