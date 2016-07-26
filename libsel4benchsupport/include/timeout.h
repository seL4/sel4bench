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
    ccnt_t server_to_handler[N_RUNS];
    ccnt_t handle_timeout[N_RUNS];
    ccnt_t ccnt_overhead[N_RUNS];
    ccnt_t handle_timeout_overhead[N_RUNS];
} timeout_results_t;
