/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef __SELBENCH_HARDWARE_H
#define __SELBENCH_HARDWARE_H

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct hardware_results {
    ccnt_t nullSyscall_results[N_RUNS];
    ccnt_t nullSyscall_overhead[N_RUNS];
} hardware_results_t;

#endif /* __SELBENCH_HARDWARE_H */
