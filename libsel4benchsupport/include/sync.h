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
#ifndef __SELBENCH_SYNC_H
#define __SELBENCH_SYNC_H

#include <sel4bench/sel4bench.h>

#define N_IGNORED 5
#define N_RUNS (50 + N_IGNORED)
#define N_WAITERS 25

#define FIFO_SIZE 1

#define N_BROADCAST_BENCHMARKS 1
#define N_PROD_CONS_BENCHMARKS 1

/* Key:
 * A - use (A)tomics instead of taking the lock after wake up
 * B - use lock release before signal in (B)roadcast
 * C - use (C)ombined syscals (SignalRecv)
 */

const char* broadcast_wait_names[] = {
    "Wait time"
};

const char* broadcast_broadcast_names[] = {
    "Broadcast time"
};

const char* producer_to_consumer_names[] = {
    "Producer to consumer"
};

const char* consumer_to_producer_names[] = {
    "Consumer to producer"
};

typedef struct sync_results {
    ccnt_t broadcast_wait_time[N_BROADCAST_BENCHMARKS][N_WAITERS][N_RUNS];
    ccnt_t broadcast_broadcast_time[N_BROADCAST_BENCHMARKS][N_RUNS];

    ccnt_t producer_to_consumer[N_PROD_CONS_BENCHMARKS][N_RUNS];
    ccnt_t consumer_to_producer[N_PROD_CONS_BENCHMARKS][N_RUNS];
} sync_results_t;

typedef void (*helper_func_t)(int argc, char *argv[]);

#endif /* __SELBENCH_SYNC_H */
