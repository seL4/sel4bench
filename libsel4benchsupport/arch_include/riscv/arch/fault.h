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

#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("a0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("a1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("a7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("a6") = (seL4_Word) ro; \
    register seL4_Word mr0 asm("a2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno), "r" (ro_copy)\
        : "a3", "a4", "a5" \
    ); \
    ip = mr0; \
} while(0)

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, swi) do { \
    register seL4_Word src asm("a0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("a1") = seL4_MessageInfo_new(0, 0, 0, 0); \
    register seL4_Word scno asm("a7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("a6") = (seL4_Word) ro; \
    register seL4_Word mr0 asm("a2"); \
    register seL4_Word mr1 asm("a3"); \
    register seL4_Word mr2 asm("a4"); \
    register seL4_Word mr3 asm("a5"); \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0), "+r" (mr1), "+r" (mr2), "+r" (mr3) \
        : "r"(scno), "r" (ro_copy) \
    ); \
    m0 = mr0; \
    m1 = mr1; \
    m2 = mr2; \
    m3 = mr3; \
} while(0)

#else
static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, UNUSED seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}

#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("a0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("a1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("a7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("a2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno) \
        : "a3", "a4", "a5" \
    ); \
    ip = mr0; \
} while(0)

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, swi) do { \
    register seL4_Word src asm("a0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("a1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("a7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("a2"); \
    register seL4_Word mr1 asm("a3"); \
    register seL4_Word mr2 asm("a4"); \
    register seL4_Word mr3 asm("a5"); \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0), "+r" (mr1), "+r" (mr2), "+r" (mr3) \
        : "r"(scno) \
    ); \
    m0 = mr0; \
    m1 = mr1; \
    m2 = mr2; \
    m3 = mr3; \
} while(0)

#endif /* CONFIG_KERNEL_MCS */

#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "ecall")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro)  DO_REPLY_RECV_1(ep, mr0, ro, "nop")
#define DO_REAL_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, "ecall")
#define DO_NOP_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, "nop")

#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#else /* __riscv_xlen == 64 */
#define LOAD  ld
#define STORE sd
#endif

#define LOAD_S STRINGIFY(LOAD)
#define STORE_S STRINGIFY(STORE)
