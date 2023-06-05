/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <autoconf.h>

#ifdef CONFIG_KERNEL_MCS
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

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 0); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("r6") = (seL4_Word) ro; \
    register seL4_Word mr0 asm("r2"); \
    register seL4_Word mr1 asm("r3"); \
    register seL4_Word mr2 asm("r4"); \
    register seL4_Word mr3 asm("r5"); \
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

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("r2"); \
    register seL4_Word mr1 asm("r3"); \
    register seL4_Word mr2 asm("r4"); \
    register seL4_Word mr3 asm("r5"); \
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

#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "swi $0")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro)  DO_REPLY_RECV_1(ep, mr0, ro, "nop")
#define DO_REAL_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, "swi $0")
#define DO_NOP_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, "nop")