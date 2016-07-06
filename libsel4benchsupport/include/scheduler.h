/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __SELBENCH_SCHEDULER_H
#define __SELBENCH_SCHEDULER_H

#include <utils/util.h>
#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)
#define N_PRIOS ((seL4_MaxPrio + seL4_WordBits - 1) / seL4_WordBits)

/* max no of threads for the criticality mode switch benchmark */
#define NUM_THREAD_SIZES 9
#define NUM_THREADS (BIT(NUM_THREAD_SIZES-1))

#define UP 0
#define DOWN 1
#define NUM_MODE_SWITCHES 2

typedef struct scheduler_results_t {
    ccnt_t thread_results[N_PRIOS][N_RUNS];
    ccnt_t process_results[N_PRIOS][N_RUNS];
    ccnt_t overhead_signal[N_RUNS];
    ccnt_t modeswitch_vary_lo[NUM_MODE_SWITCHES][NUM_THREAD_SIZES][N_RUNS];
    ccnt_t modeswitch_vary_hi[NUM_MODE_SWITCHES][NUM_THREAD_SIZES][N_RUNS];
    ccnt_t thread_yield[N_RUNS];
    ccnt_t process_yield[N_RUNS];
    ccnt_t overhead_ccnt[N_RUNS];
} scheduler_results_t;

static inline uint8_t
gen_next_prio(int i)
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

#endif /* __SELBENCH_SCHEDULER_H */
