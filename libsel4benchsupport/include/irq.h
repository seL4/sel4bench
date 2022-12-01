/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include "benchmark.h"

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct irq_results {
    kernel_log_entry_t kernel_log[KERNEL_MAX_NUM_LOG_ENTRIES];
    int n;
} irq_results_t;

typedef struct irquser_results_t {
    ccnt_t overheads[N_RUNS];
    ccnt_t thread_results[N_RUNS];
    ccnt_t process_results[N_RUNS];

    /* Data for early processing */
    ccnt_t overhead_min;

    ccnt_t thread_results_ep_sum;
    ccnt_t thread_results_ep_sum2;
    ccnt_t thread_results_ep_num;
    ccnt_t thread_results_ep[N_RUNS];

    ccnt_t process_results_ep_sum;
    ccnt_t process_results_ep_sum2;
    ccnt_t process_results_ep_num;
    ccnt_t process_results_ep[N_RUNS];
} irquser_results_t;
