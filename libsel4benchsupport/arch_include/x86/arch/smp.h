/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <sel4_arch/smp.h>

#define CACHE_LN_SZ CONFIG_CACHE_LN_SZ

#define READ_CYCLE_COUNTER(_r) do {         \
    (_r) = sel4bench_get_cycle_count();     \
} while(0)
