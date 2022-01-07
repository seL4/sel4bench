/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct {
    ccnt_t reply_recv_1_overhead[N_RUNS];
    ccnt_t ccnt_overhead[N_RUNS];

    /* we ignore the last result for the following,
     * but need to be able to write to the buffer without
     * overriding anything,
     * so add 1 to the number recorded */
    ccnt_t round_trip[N_RUNS + 1];
    ccnt_t fault[N_RUNS + 1];
    ccnt_t fault_reply[N_RUNS + 1];
    ccnt_t vm_fault[N_RUNS + 1];
    ccnt_t vm_fault_reply[N_RUNS + 1];
    ccnt_t vm_fault_map[N_RUNS + 1];
} fault_results_t;
