/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4bench/sel4bench.h>
#include <benchmark.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct signal_results {
    /* Data for late processing */
    ccnt_t lo_prio_results[N_RUNS];
    ccnt_t hi_prio_results[N_RUNS];
    ccnt_t hi_prio_average[N_RUNS][NUM_AVERAGE_EVENTS];
    ccnt_t overhead[N_RUNS]; /* "overhead" is used by early processing as well */
    /* Data for early processing */
    ccnt_t lo_sum; /* sum of samples */
    ccnt_t lo_sum2; /* sum of squared samples */
    ccnt_t lo_num; /* number of samples to process */
    ccnt_t overhead_min; /* min overhead found in "overhead" array */
    /* array required by report output function
    Zeros, but can be used for diagnostic data */
    ccnt_t diag_results[N_RUNS]; /* array required by report output function */
} signal_results_t;
