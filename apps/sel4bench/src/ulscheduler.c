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

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/device.h>

#include <cspace.h>
#include <stdio.h>
#include <ulscheduler.h>

static json_t *
ulscheduler_process(void *results) {
    ulscheduler_results_t *raw_results = results;

    json_t *array = json_array();

    /* process overhead */
    result_desc_t desc = {
        .stable = true,
        .name = "ulscheduler measurement overhead",
        .ignored = N_IGNORED,
    };

    result_t result = process_result(N_RUNS, raw_results->overhead, desc);
    result_set_t set = {
        .name = "Ulscheduler measurement overhead",
        .n_extra_cols = 0,
        .results = &result,
        .n_results = 1,
    };
    json_array_append_new(array, result_set_to_json(set));

    /* set up num task column */
    json_int_t column_values[CONFIG_NUM_TASK_SETS];
    for (json_int_t i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        column_values[i] = i + CONFIG_MIN_TASKS;
    }

    /* describe extra column */
    column_t extra = {
        .header = "Tasks",
        .type = JSON_INTEGER,
        .integer_array = &column_values[0],
    };

    /* describe result sets for ulscheduler results */
    result_t per_task_results[CONFIG_NUM_TASK_SETS];
    set.name = "EDF (clients call when done)";
    set.extra_cols = &extra;
    set.n_extra_cols = 1;
    set.results = per_task_results;
    set.n_results = CONFIG_NUM_TASK_SETS;

    /* results won't be stable */
    desc.stable = false;
    /* set min overhead calculated above */
    desc.overhead = result.min;

    process_results(CONFIG_NUM_TASK_SETS, N_RUNS, raw_results->edf_coop, desc, per_task_results);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "EDF (preempt)";
    process_results(CONFIG_NUM_TASK_SETS, N_RUNS, raw_results->edf_preempt, desc, per_task_results);
    json_array_append_new(array, result_set_to_json(set));

    set.name = "CFS (clients call when done)";
    process_results(CONFIG_NUM_TASK_SETS, N_RUNS, raw_results->cfs_coop, desc, per_task_results);
    json_array_append_new(array, result_set_to_json(set));

    return array;
}

static benchmark_t ulsched_benchmark = {
    .name = "ulscheduler",
    .enabled = config_set(CONFIG_APP_ULSCHEDULERBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(ulscheduler_results_t), seL4_PageBits),
    .process = ulscheduler_process,
    .init = blank_init
};

benchmark_t *
ulscheduler_benchmark_new(void)
{
    return &ulsched_benchmark;
}

