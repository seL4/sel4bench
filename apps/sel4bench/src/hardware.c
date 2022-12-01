/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <hardware.h>
#include <stdio.h>

static json_t *hardware_process(void *results)
{
    hardware_results_t *raw_results = results;

    result_desc_t desc = {
        .stable = false,
        .name = "Nop syscall overhead",
        .ignored = N_IGNORED
    };

    result_t nopnulsyscall_result = process_result(N_RUNS, raw_results->nullSyscall_overhead, desc);

    /* Execlude ccnt, user-level and loop overheads */
    desc.overhead = nopnulsyscall_result.min;

    result_t result = process_result(N_RUNS, raw_results->nullSyscall_results, desc);

    result_set_t set = {
        .name = "Hardware null_syscall thread",
        .n_results = 1,
        .n_extra_cols = 0,
        .results = &result
    };

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Hardware null_syscall thread (early processing)";
    result = process_result_early_proc(raw_results->nullSyscall_ep_num, raw_results->nullSyscall_ep_sum,
                                       raw_results->nullSyscall_ep_sum2, raw_results->nullSyscall_ep);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "Nop syscall overhead";
    set.results = &nopnulsyscall_result;
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t hardware_benchmark = {
    .name = "hardware",
    .enabled = config_set(CONFIG_APP_HARDWAREBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(hardware_results_t), seL4_PageBits),
    .process = hardware_process,
    .init = blank_init
};

benchmark_t *hardware_benchmark_new(void)
{
    return &hardware_benchmark;
}
