/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>

#include "benchmark.h"
#include "processing.h"
#include "json.h"

#include <irq.h>
#include <sel4platsupport/device.h>
#include <stdio.h>
#include <utils/time.h>

static json_t *irquser_process(void *r)
{
    irquser_results_t *raw_results = r;

    result_desc_t desc = {
        .ignored = N_IGNORED,
        .name = "IRQ user measurement overhead"
    };

    result_t results[5];

    results[0] = process_result(N_RUNS, raw_results->overheads, desc);

    desc.overhead = results[0].min;

    results[1] = process_result(N_RUNS, raw_results->thread_results, desc);
    results[2] = process_result_early_proc(raw_results->thread_results_ep_num,
                                           raw_results->thread_results_ep_sum,
                                           raw_results->thread_results_ep_sum2,
                                           raw_results->thread_results_ep);
    results[3] = process_result(N_RUNS, raw_results->process_results, desc);
    results[4] = process_result_early_proc(raw_results->process_results_ep_num,
                                           raw_results->process_results_ep_sum,
                                           raw_results->process_results_ep_sum2,
                                           raw_results->process_results_ep);

    char *types[] = {"Measurement overhead", "Without context switch",
                     "Without context switch (early processing)", "With context switch",
                     "With context switch (early processing)"
                    };

    column_t col = {
        .header = "Type",
        .type = JSON_STRING,
        .string_array = types
    };

    result_set_t set = {
        .name = "IRQ path cycle count (measured from user level)",
        .n_results = 5,
        .results = results,
        .n_extra_cols = 1,
        .extra_cols = &col
    };

    json_t *json = json_array();
    json_array_append_new(json, result_set_to_json(set));
    return json;
}

static benchmark_t irquser_benchmark = {
    .name = "irquser",
    .enabled = config_set(CONFIG_APP_IRQUSERBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(irquser_results_t), seL4_PageBits),
    .process = irquser_process,
    .init = blank_init
};

benchmark_t *irquser_benchmark_new(void)
{
    return &irquser_benchmark;
}
