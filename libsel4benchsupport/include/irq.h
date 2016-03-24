/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __SELBENCH_IRQ_H
#define __SELBENCH_IRQ_H

#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include "benchmark.h"

#define N_IGNORED 10
#define RUNS (100 + N_IGNORED)

typedef struct irq_results {
    kernel_log_entry_t kernel_log[KERNEL_MAX_NUM_LOG_ENTRIES];
    int n;
} irq_results_t;

#endif /* __SELBENCH_IRQ_H */
