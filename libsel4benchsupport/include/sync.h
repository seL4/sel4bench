/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#ifndef __SELBENCH_SYNC_H
#define __SELBENCH_SYNC_H

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct sync_results {
    ccnt_t lo_prio_results[N_RUNS];
    ccnt_t hi_prio_results[N_RUNS];
    ccnt_t overhead[N_RUNS];
} sync_results_t;

#endif /* __SELBENCH_SYNC_H */
