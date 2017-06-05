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
