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

#include <timeout.h>
#include <stdio.h>

static json_t *
timeout_process(void *results) {
    timeout_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = true,
        .name = "ccnt overhead",
        .ignored = N_IGNORED
    };

    result_t result = process_result(N_RUNS, raw_results->ccnt_overhead, desc);

    result_set_t set = {
        .name = "ccnt overhead",
        .n_results = 1,
        .n_extra_cols = 0,
        .results = &result
    };

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(set));

    desc.stable = false;
    desc.overhead = result.min;

    set.name = "timeout: server to handler";
    result = process_result(N_RUNS, raw_results->server_to_handler, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "handle timeout overhead";
    desc.stable = true;
    desc.overhead = 0;
    result = process_result(N_RUNS, raw_results->handle_timeout_overhead, desc);
    json_array_append_new(array, result_set_to_json(set));

    /* timeout to timeout handler does not */
    set.name = "handle timeout";
    desc.stable = false;
    desc.overhead = result.min;
    result = process_result(N_RUNS, raw_results->handle_timeout, desc);
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t timeout_benchmark = {
    .name = "timeout",
    .enabled = config_set(CONFIG_APP_TIMEOUTBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(timeout_results_t), seL4_PageBits),
    .process = timeout_process,
    .init = blank_init
};

benchmark_t *
timeout_benchmark_new(void)
{
    return &timeout_benchmark;
}

