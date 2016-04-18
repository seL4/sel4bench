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

#include <scheduler.h>
#include <stdio.h>

static void 
scheduler_init(UNUSED vka_t *vka, UNUSED simple_t *simple, UNUSED sel4utils_process_t *process) 
{
    /* nothing to do */
}

static void
print_prio_result(result_t *result)
{
    printf("Prio\t");
    print_result_header();
    int prio = seL4_MinPrio + 1;
    for (int i = 0; i < N_PRIOS; i++) {
        printf("%d\t", prio);
        prio += seL4_WordBits;
        print_result(&result[i]);
    }
}

static void 
scheduler_process(void *results) {
    scheduler_results_t *raw_results;
    result_t inter_as[N_PRIOS];
    result_t intra_as[N_PRIOS];
    result_t thread_yield;
    result_t process_yield;
    result_t signal_overhead;
    result_t yield_overhead;

    raw_results = (scheduler_results_t *) results;

    signal_overhead = process_result(&raw_results->overhead_signal[N_IGNORED], N_RUNS - N_IGNORED, "signal overhead");
    yield_overhead = process_result(&raw_results->overhead_yield[N_IGNORED], N_RUNS - N_IGNORED, "yield overhead");
  
    /* account for overhead */
    for (int i = 0; i < N_PRIOS; i++) {
        for (int j = 0; j < N_RUNS; j++) {
            raw_results->thread_results[i][j] -= signal_overhead.min;
            raw_results->process_results[i][j] -= signal_overhead.min;
        }
        raw_results->thread_yield[i] -= yield_overhead.min;
        raw_results->process_yield[i] -= yield_overhead.min;
    }

    for (int i = 0; i < N_PRIOS; i++) {
        intra_as[i] = process_result(&raw_results->thread_results[i][N_IGNORED],
                                     N_RUNS - N_IGNORED, "thread switch");
        inter_as[i] = process_result(&raw_results->process_results[i][N_IGNORED],
                                     N_RUNS - N_IGNORED, "process switch");
    }

    thread_yield = process_result(&raw_results->thread_yield[N_IGNORED], N_RUNS - N_IGNORED, 
                                  "thread yield");
    process_yield = process_result(&raw_results->process_yield[N_IGNORED], N_RUNS - N_IGNORED, 
                                  "process yield");

    print_banner("Signal overhead", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&signal_overhead);
    
    print_banner("Yield overhead", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&yield_overhead);
    
    print_banner("Signal to thread of higher prio", N_RUNS - N_IGNORED);
    print_prio_result(intra_as);
    
    print_banner("Signal to process of higher prio", N_RUNS - N_IGNORED);
    print_prio_result(inter_as);
    
    print_banner("Thread yield", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&thread_yield);
    
    print_banner("Process yield", N_RUNS - N_IGNORED);
    print_result_header();
    print_result(&process_yield);
}

static benchmark_t sched_benchmark = {
    .name = "scheduler",
    .enabled = config_set(CONFIG_APP_SCHEDULERBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(scheduler_results_t), seL4_PageBits),
    .process = scheduler_process,
    .init = scheduler_init
};

benchmark_t *
scheduler_benchmark_new(void) 
{
    return &sched_benchmark;
}

