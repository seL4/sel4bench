/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __SELBENCH_SIGNAL_H
#define __SELBENCH_SIGNAL_H

#include <sel4bench/sel4bench.h>
#include <benchmark.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)

typedef struct signal_results {
    ccnt_t lo_prio_results[N_RUNS];
    ccnt_t hi_prio_results[N_RUNS];
    ccnt_t overhead[N_RUNS];
    ccnt_t hi_prio_average[NUM_AVERAGE_EVENTS];
} signal_results_t;

#endif /* __SELBENCH_SIGNAL_H */
