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
#define N_PRIOS ((seL4_MaxPrio + seL4_WordBits - 1) / seL4_WordBits)

typedef struct scheduler_results_t {
    ccnt_t thread_results[N_PRIOS][N_RUNS];
    ccnt_t process_results[N_PRIOS][N_RUNS];
    ccnt_t overhead_signal[N_RUNS];
    ccnt_t set_prio_average[N_RUNS][NUM_AVERAGE_EVENTS];

    ccnt_t thread_yield[N_RUNS];
    ccnt_t process_yield[N_RUNS];
    ccnt_t overhead_ccnt[N_RUNS];
    ccnt_t average_yield[N_RUNS][NUM_AVERAGE_EVENTS];

    /* Data for early processing */
    ccnt_t overhead_ccnt_min;

    ccnt_t thread_yield_ep_sum;
    ccnt_t thread_yield_ep_sum2;
    ccnt_t thread_yield_ep_num;
    ccnt_t thread_yield_ep[N_RUNS];

    ccnt_t process_yield_ep_sum;
    ccnt_t process_yield_ep_sum2;
    ccnt_t process_yield_ep_num;
    ccnt_t process_yield_ep[N_RUNS];
} scheduler_results_t;

static inline uint8_t gen_next_prio(int i)
{
    /* for the master seL4 kernel, the only thing that effects the length of a
     * schedule call is how far apart the two prios are that we are switching between.
     *
     * So for this benchmark we record the amount of time taken for a
     * seL4_Signal to take place, causing a higher priority thread to run.
     *
     * Since the scheduler hits a different word in the 2nd level bitmap every wordBits
     * priority, we only test each wordBits prio.
     */
    return seL4_MinPrio + 1 + (i * seL4_WordBits);
}
