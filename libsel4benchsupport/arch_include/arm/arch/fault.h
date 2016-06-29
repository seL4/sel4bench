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

#include <sel4bench/armv/sel4bench.h>

#define READ_COUNTER_BEFORE SEL4BENCH_READ_CCNT
#define READ_COUNTER_AFTER  SEL4BENCH_READ_CCNT

#define DO_REPLY_RECV_1(ep, ip, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("r2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : [swi_num] "i" __SEL4_SWINUM(seL4_SysReplyRecv), "r"(scno) \
    ); \
    ip = mr0; \
} while(0)

#define DO_REAL_REPLY_RECV_1(ep, mr0) DO_REPLY_RECV_1(ep, mr0, "swi %[swi_num]")
#define DO_NOP_REPLY_RECV_1(ep, mr0)  DO_REPLY_RECV_1(ep, mr0, "nop")

static inline seL4_MessageInfo_t
seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0) {
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL);
}

static inline void
seL4_ReplyWith1MR(seL4_Word mr0)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}

