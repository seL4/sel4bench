/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <autoconf.h>
#include <benchmark.h>

#define CACHE_LN_SZ 32
#define OVERHEAD_MEAS_RUNS 16

#define READ_CYCLE_COUNTER(_r) do {                                 \
    SEL4BENCH_READ_CCNT(_r);                                        \
} while(0)
#define RESET_CYCLE_COUNTER SEL4BENCH_RESET_CCNT
#define OVERHEAD_FIXUP(_c, _o) ((_c) - (_o))

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

#ifndef CONFIG_ARCH_ARM_V6
/* smp_benchmark_check_overhead is not supported on armv6 platforms */
static inline ccnt_t smp_benchmark_check_overhead(void)
{
    uint32_t ccnt_overhead;
    uint32_t begin, end, total = 0;

    sel4bench_init();
    for (int i = 0; i < OVERHEAD_MEAS_RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        READ_CYCLE_COUNTER(begin);

        READ_CYCLE_COUNTER(end);
        COMPILER_MEMORY_FENCE();
        total += (end - begin);
    }
    ccnt_overhead = total / OVERHEAD_MEAS_RUNS;
    total = 0;
    for (int i = 0; i < OVERHEAD_MEAS_RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        RESET_CYCLE_COUNTER;

        READ_CYCLE_COUNTER(end);
        COMPILER_MEMORY_FENCE();
        total += (end - ccnt_overhead);
    }
    sel4bench_destroy();

    return total / OVERHEAD_MEAS_RUNS;
}
#endif /* !CONFIG_ARCH_ARM_V6 */
