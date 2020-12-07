/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <autoconf.h>

#define READ_COUNTER_BEFORE SEL4BENCH_READ_CCNT
#define READ_COUNTER_AFTER  SEL4BENCH_READ_CCNT

#ifdef CONFIG_KERNEL_MCS
static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, reply, mr0, NULL, NULL, NULL);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, seL4_CPtr dest)
{
    return seL4_SendWithMRs(dest, seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}
#else
static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, UNUSED seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}
#endif /* CONFIG_KERNEL_MCS */
