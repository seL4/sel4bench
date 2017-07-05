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

#include <autoconf.h>
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <utils/util.h>
#include <smp.h>

#include "benchmark.h"
#include "json.h"
#include "math.h"
#include "printing.h"
#include "processing.h"

static int cores_collective_results;

static void
process_smp_results_init(UNUSED vka_t *vka, simple_t *simple, UNUSED sel4utils_process_t *process)
{
    cores_collective_results = simple_get_core_count(simple);
}

static json_t *
process_smp_results(void *r)
{
    smp_results_t *raw_results = r;

    int n = TESTS * cores_collective_results;

    json_int_t cycle_col[n], cores_col[n];
    for (int i = 0; i < n; i++) {
        cycle_col[i] = smp_benchmark_params[i / cores_collective_results].delay;
        cores_col[i] = (i % cores_collective_results) + 1;
    }

    column_t extra_cols[] = {
        {
            .header = "Cycles",
            .type = JSON_INTEGER,
            .integer_array = cycle_col,
        },
        {
            .header = "Cores",
            .type = JSON_INTEGER,
            .integer_array = cores_col,
        },
    };

    result_t results[TESTS][cores_collective_results];

    result_set_t result_set = {
        .name = "SMP Benchmark",
        .extra_cols = extra_cols,
        .n_extra_cols = ARRAY_SIZE(extra_cols),
        .results = (result_t *) results,
        .n_results = n,
    };

    for (int i = 0; i < TESTS; i++) {
        for (int j = 0; j < cores_collective_results; j++) {
            result_desc_t desc = {
                .name = smp_benchmark_params[i].name,
                .overhead = 0,
            };
            results[i][j] = process_result(RUNS, raw_results->benchmarks_result[i][j], desc);
        }
    }

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(result_set));
    return array;
}

static benchmark_t smp_benchmark = {
    .name = "smp",
    .enabled = config_set(CONFIG_APP_SMPBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(smp_results_t), seL4_PageBits),
    .process = process_smp_results,
    .init = process_smp_results_init
};

benchmark_t *
smp_benchmark_new(void)
{
    return &smp_benchmark;
}
