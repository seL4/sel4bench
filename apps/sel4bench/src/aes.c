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

#include <aes.h>
#include <stdio.h>

static json_t *
aes_process(void *results)
{
    aes_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = true,
        .name = "aes overhead",
        .ignored = N_IGNORED
    };

    result_t result = process_result(N_RUNS, raw_results->overhead, desc);

    result_set_t set = {
        .name = "aes overhead",
        .n_results = 1,
        .n_extra_cols = 0,
        .results = &result
    };

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes rollback";
    desc.stable = false;
    desc.overhead = result.min;

    result = process_result(N_RUNS, raw_results->rollback_cost, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes rollback cold";
    result = process_result(N_RUNS, raw_results->rollback_cost_cold, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes emergency";
    result = process_result(N_RUNS, raw_results->emergency_cost, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes emergency cold";
    result = process_result(N_RUNS, raw_results->emergency_cost_cold, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes extend";
    result = process_result(N_RUNS, raw_results->extend_cost, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "aes extend cold";
    result = process_result(N_RUNS, raw_results->extend_cost_cold, desc);
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t aes_benchmark = {
    .name = "aes",
    .enabled = config_set(CONFIG_APP_AES),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(aes_results_t), seL4_PageBits),
    .process = aes_process,
    .init = blank_init
};

benchmark_t *
aes_benchmark_new(void)
{
    return &aes_benchmark;
}

