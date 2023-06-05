/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <fault.h>
#include <stdio.h>

static json_t *fault_process(void *results)
{
    fault_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = true,
        .name = "fault overhead",
        .ignored = N_IGNORED
    };

    /* calculate overhead of reply_recv */
    result_t result = process_result(N_RUNS, raw_results->reply_recv_1_overhead, desc);

    result_set_t set = {
        .name = "fault overhead (undefined instruction fault)",
        .n_results = 1,
        .n_extra_cols = 0,
        .results = &result
    };

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(set));

    desc.stable = false;
    desc.overhead = result.min;

    set.name = "fault round trip";
    result = process_result(N_RUNS, raw_results->round_trip, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "fault round trip (early processing)";
    result = process_result_early_proc(raw_results->round_trip_ep_num, raw_results->round_trip_ep_sum,
                                       raw_results->round_trip_ep_sum2, raw_results->round_trip_ep);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "faulter -> fault handler";
    result = process_result(N_RUNS, raw_results->fault, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "faulter -> fault handler (early processing)";
    result = process_result_early_proc(raw_results->fault_ep_num, raw_results->fault_ep_sum,
                                       raw_results->fault_ep_sum2, raw_results->fault_ep);
    json_array_append_new(array, result_set_to_json(set));

    /* Get the overhead for VM faults */
    set.name = "fault overhead (vm fault)";
    desc.stable = true;
    desc.overhead = 0;
    result = process_result(N_RUNS, raw_results->reply_0_recv_4_overhead, desc);
    json_array_append_new(array, result_set_to_json(set));

    desc.stable = false;
    desc.overhead = result.min;

    set.name = "vm faulter -> fault handler";
    result = process_result(N_RUNS, raw_results->vm_fault, desc);
    json_array_append_new(array, result_set_to_json(set));

    /* calculate the overhead of reading the cycle count (fault handler -> faulter path
     * does not include a call to seL4_ReplyRecv_ */

    set.name = "read ccnt overhead";
    desc.stable = true;
    desc.overhead = 0;
    result = process_result(N_RUNS, raw_results->ccnt_overhead, desc);
    json_array_append_new(array, result_set_to_json(set));

    int ccnt_overhead = result.min;

    /* fault to fault handler does not */
    set.name = "fault handler -> faulter";
    desc.stable = false;
    desc.overhead = ccnt_overhead;
    result = process_result(N_RUNS, raw_results->fault_reply, desc);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "fault handler -> faulter (early processing)";
    result = process_result_early_proc(raw_results->fault_reply_ep_num, raw_results->fault_reply_ep_sum,
                                       raw_results->fault_reply_ep_sum2, raw_results->fault_reply_ep);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "vm fault handler -> faulter";
    result = process_result(N_RUNS, raw_results->vm_fault_reply, desc);
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t fault_benchmark = {
    .name = "fault",
    .enabled = config_set(CONFIG_APP_FAULTBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(fault_results_t), seL4_PageBits),
    .process = fault_process,
    .init = blank_init
};

benchmark_t *fault_benchmark_new(void)
{
    return &fault_benchmark;
}
