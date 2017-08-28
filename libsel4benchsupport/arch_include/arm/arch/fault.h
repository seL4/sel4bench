/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once

#include <autoconf.h>
#include <sel4bench/armv/sel4bench.h>

#define READ_COUNTER_BEFORE SEL4BENCH_READ_CCNT
#define READ_COUNTER_AFTER  SEL4BENCH_READ_CCNT

#ifdef CONFIG_KERNEL_RT
#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("r6") = (seL4_Word) ro; \
    register seL4_Word mr0 asm("r2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno), "r" (ro_copy)\
        : "r3", "r4", "r5" \
    ); \
    ip = mr0; \
} while(0)


static inline seL4_MessageInfo_t
seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, seL4_CPtr reply) {
    return seL4_RecvWithMRs(src, NULL, reply, mr0, NULL, NULL, NULL);
}

static inline void
seL4_ReplyWith1MR(seL4_Word mr0, seL4_CPtr dest)
{
    return seL4_SendWithMRs(dest, seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}
#else
#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("r2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno) \
        : "r3", "r4", "r5" \
    ); \
    ip = mr0; \
} while(0)

static inline seL4_MessageInfo_t
seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, UNUSED seL4_CPtr reply) {
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL);
}

static inline void
seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}
#endif /* CONFIG_KERNEL_RT */

#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "swi $0")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro)  DO_REPLY_RECV_1(ep, mr0, ro, "nop")
