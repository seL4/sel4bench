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
process_result_set(char *name, result_t overhead, ccnt_t raw_results[CONFIG_NUM_TASK_SETS][N_RUNS])
{
    result_t results[CONFIG_NUM_TASK_SETS];

    /* account for overhead */
    for (int i = 0; i < CONFIG_NUM_TASK_SETS; i++) {
        for (int j = 0; j < N_RUNS; j++) {
            raw_results[i][j] -= overhead.min;
        }
        results[i] = process_result_ignored(raw_results[i], N_RUNS, N_IGNORED, name);
    }

    print_banner(name, N_RUNS - N_IGNORED);
    print_task_results(results);
}


static void
ulscheduler_process(void *results) {
    ulscheduler_results_t *raw_results;
    result_t overhead;

    raw_results = (ulscheduler_results_t *) results;

    overhead = process_result_ignored(raw_results->overhead, N_RUNS, N_IGNORED, "EDF overhead");
    process_result_set("EDF (clients call when done)", overhead, raw_results->edf_coop);
    process_result_set("EDF (preempt)", overhead, raw_results->edf_preempt);

    print_banner("ulscheduler measurement overhead", N_RUNS - N_IGNORED);
    print_result(&overhead);
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

