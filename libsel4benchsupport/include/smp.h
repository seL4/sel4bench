/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <arch/smp.h>

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
