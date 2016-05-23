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
#include "printing.h"

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/device.h>

#include <cspace.h>
#include <stdio.h>
#include <ulscheduler.h>

static void
print_task_results(result_t *results)
{
    printf("Tasks\t");
    print_result_header();
    for (int i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        printf("%zu\t", i + CONFIG_MIN_TASKS);
        print_result(&results[i]);
    }
}

static void 
ulscheduler_process(void *results) {
    ulscheduler_results_t *raw_results;
    result_t overhead;
    result_t edf_coop[CONFIG_NUM_TASK_SETS];

    raw_results = (ulscheduler_results_t *) results;

    overhead = process_result_ignored(raw_results->overhead, N_RUNS, N_IGNORED, "EDF overhead");

    /* account for overhead */
    for (int i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        for (int j = 0; j < N_RUNS; j++) {
            raw_results->edf_coop[i][j] -= overhead.min;
        }
        edf_coop[i] = process_result_ignored(raw_results->edf_coop[i], 
                                              N_RUNS, N_IGNORED, "EDF coop");
    }

    print_banner("EDF measurement overhead", N_RUNS - N_IGNORED);
    print_result(&overhead);
    print_banner("EDF (clients call when done)", N_RUNS - N_IGNORED);
    print_task_results(edf_coop);
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

