/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>
#include <vcpu.h>
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <utils/util.h>

#include "benchmark.h"

#ifndef CONFIG_APP_VCPU_BENCH
static benchmark_t vcpu_benchmark = {
    .name = "vcpu",
    .enabled = config_set(CONFIG_APP_VCPU_BENCH),
    .results_pages = 0,
    .process = NULL,
    .init = blank_init
};

benchmark_t *vcpu_benchmark_new(void)
{
    return &vcpu_benchmark;
}
#endif
