/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#include <assert.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "timing.h"

static char timing_ready = false;

void
timing_init(void)
{
    sel4bench_init();

#if TIMING_MULTIPLE_PMCS
    /* Enable PMC0 and PMC1 if asked for */
    assert(sel4bench_get_num_counters() >= 2);
    sel4bench_set_count_event(0, SEL4BENCH_EVENT_CACHE_L1D_MISS);
    sel4bench_set_count_event(1, SEL4BENCH_EVENT_CACHE_L1I_MISS);
    sel4bench_start_counters(0x3);
    sel4bench_reset_counters(0x3);
#endif /* TIMING_MULTIPLE_PMCS */

    timing_ready = true;
}

void
timing_destroy(void)
{
    sel4bench_destroy();
    timing_ready = false;
}

