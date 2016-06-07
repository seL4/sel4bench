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

#include <autoconf.h>
#include <sel4bench/sel4bench.h>


#ifndef CONFIG_MIN_TASKS
#define CONFIG_MIN_TASKS 0
#endif

#ifndef CONFIG_NUM_TASK_SETS
#define CONFIG_NUM_TASK_SETS 0
#endif

#ifndef CONFIG_NUM_RUNS
#define CONFIG_NUM_RUNS 0
#endif

#define N_IGNORED 1
#define N_RUNS (CONFIG_NUM_RUNS + N_IGNORED)

typedef struct ulscheduler_results_t {
    ccnt_t edf_coop[CONFIG_NUM_TASK_SETS][N_RUNS];
    ccnt_t edf_preempt[CONFIG_NUM_TASK_SETS][N_RUNS];
    ccnt_t overhead[N_RUNS];
} ulscheduler_results_t;

