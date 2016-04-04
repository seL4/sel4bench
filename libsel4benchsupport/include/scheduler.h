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

#include <sel4bench/sel4bench.h>

#define N_IGNORED 10
#define N_RUNS (100 + N_IGNORED)
#define N_PRIOS ((seL4_MaxPrio + seL4_WordBits - 1) / seL4_WordBits)

typedef struct scheduler_results_t {
    ccnt_t thread_results[N_PRIOS][N_RUNS];
    ccnt_t process_results[N_PRIOS][N_RUNS];
    ccnt_t overhead_signal[N_RUNS];
    
    ccnt_t thread_yield[N_RUNS];
    ccnt_t process_yield[N_RUNS];
    ccnt_t overhead_yield[N_RUNS];
} scheduler_results_t;

#endif /* __SELBENCH_SCHEDULER_H */
