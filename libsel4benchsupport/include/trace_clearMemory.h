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

typedef struct trace_clear_memory_results {
    kernel_log_entry_t kernel_log[KERNEL_MAX_NUM_LOG_ENTRIES];
    int n;
} trace_clear_memory_results_t;

