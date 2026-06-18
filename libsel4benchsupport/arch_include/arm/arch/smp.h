/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <autoconf.h>
#include <benchmark.h>

#define CACHE_LN_SZ 64

#define READ_CYCLE_COUNTER(_r) do {                                 \
    SEL4BENCH_READ_CCNT(_r);                                        \
} while(0)

static inline void smp_benchmark_ping(seL4_CPtr ep)
{
    seL4_CallWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, NULL);
}

static inline void smp_benchmark_pong(seL4_CPtr ep, seL4_CPtr reply)
{
#if CONFIG_KERNEL_MCS
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, NULL, NULL, reply);
#else
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, NULL, NULL);
#endif
}
