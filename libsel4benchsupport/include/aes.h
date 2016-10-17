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
#pragma once

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct aes_results {
    ccnt_t rollback_cost[N_RUNS];
    ccnt_t rollback_cost_cold[N_RUNS];
    ccnt_t emergency_cost[N_RUNS];
    ccnt_t emergency_cost_cold[N_RUNS];
    ccnt_t extend_cost[N_RUNS];
    ccnt_t extend_cost_cold[N_RUNS];
    ccnt_t overhead[N_RUNS];
} aes_results_t;

