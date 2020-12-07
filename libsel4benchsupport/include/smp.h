/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <autoconf.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#if CONFIG_MAX_NUM_NODES > 1
#include <arch/smp.h>
#endif

#define WARMUPS 2
#define RUNS 5
#define TESTS ARRAY_SIZE(smp_benchmark_params)

typedef struct benchmark_params {
    const char *name;
    const double delay;
} benchmark_params_t;

static const
benchmark_params_t smp_benchmark_params[] = {
    { .name = "500 cycles",   .delay = 500.000, },
    { .name = "1000 cycles",  .delay = 1000.00, },
    { .name = "2000 cycles",  .delay = 2000.00, },
    { .name = "4000 cycles",  .delay = 4000.00, },
    { .name = "8000 cycles",  .delay = 8000.00, },
    { .name = "16000 cycles", .delay = 16000.0, },
    { .name = "32000 cycles", .delay = 32000.0, },
};

typedef struct smp_results {
    ccnt_t benchmarks_result[TESTS][CONFIG_MAX_NUM_NODES][RUNS];
} smp_results_t;
