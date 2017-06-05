/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#ifndef __SELBENCH_IRQ_H
#define __SELBENCH_IRQ_H

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
} irquser_results_t;

#endif /* __SELBENCH_IRQ_H */
