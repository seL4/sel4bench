/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#pragma once

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct {
    ccnt_t reply_recv_overhead[N_RUNS];
    ccnt_t ccnt_overhead[N_RUNS];

    /* we ignore the last result for the following,
     * but need to be able to write to the buffer without
     * overriding anything,
     * so add 1 to the number recorded */
    ccnt_t round_trip[N_RUNS + 1];
    ccnt_t fault[N_RUNS + 1];
    ccnt_t fault_reply[N_RUNS + 1];
    ccnt_t round_trip_passive[N_RUNS + 1];
} fault_results_t;

