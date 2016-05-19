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

#include <signal.h>
#include <stdio.h>

static void 
signal_process(void *results) {
    signal_results_t *raw_results;
    result_t hi_result, lo_result;
    result_t overhead;

    raw_results = (signal_results_t *) results;

    overhead = process_result_ignored(raw_results->overhead, N_RUNS, N_IGNORED, "signal overhead");
 
    /* account for overhead */
    for (int j = 0; j < N_RUNS; j++) {
        raw_results->lo_prio_results[j] -= overhead.min;
        raw_results->hi_prio_results[j] -= overhead.min;
    }

    lo_result = process_result_ignored(raw_results->lo_prio_results, N_RUNS, N_IGNORED, "signal (deliver)");
    hi_result = process_result_ignored(raw_results->hi_prio_results, N_RUNS, N_IGNORED, "signal (return)");

    print_banner("Signal overhead", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&overhead);
    
    print_banner("Signal to high prio thread", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&lo_result);
    
    print_banner("Signal to low prio thread", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&hi_result);
    
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

