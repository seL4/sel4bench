/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct hardware_results {
    ccnt_t nullSyscall_results[N_RUNS];
    ccnt_t nullSyscall_overhead[N_RUNS];

    /* Data for early processing */
    ccnt_t overhead_min;

    ccnt_t nullSyscall_ep_sum;
    ccnt_t nullSyscall_ep_sum2;
    ccnt_t nullSyscall_ep_num;
    ccnt_t nullSyscall_ep[N_RUNS];
} hardware_results_t;
