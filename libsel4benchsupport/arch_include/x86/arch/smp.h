/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <sel4_arch/smp.h>

#define CACHE_LN_SZ CONFIG_CACHE_LN_SZ

#define READ_CYCLE_COUNTER(_r) do {         \
    (_r) = sel4bench_get_cycle_count();     \
} while(0)
#define RESET_CYCLE_COUNTER
#define OVERHEAD_FIXUP(_c, _o) (_c)

static inline ccnt_t smp_benchmark_check_overhead(void)
{
    return 0;
}
