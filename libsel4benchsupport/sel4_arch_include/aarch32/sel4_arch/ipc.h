/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <autoconf.h>

#define DO_CALL(ep, tag, swi) do { \
    register seL4_Word dest asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysCall; \
    asm volatile(NOPS swi NOPS \
        : "+r"(dest), "+r"(info) \
        : "r"(scno) \
    ); \
} while(0)

#define DO_CALL_10(ep, tag, swi) do { \
    register seL4_Word dest asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysCall; \
    asm volatile(NOPS swi NOPS \
        : "+r"(dest), "+r"(info) \
        : "r"(scno) \
        : "r2", "r3", "r4", "r5" \
    ); \
} while(0)

#define DO_SEND(ep, tag, swi) do { \
    register seL4_Word dest asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysSend; \
    asm volatile(NOPS swi NOPS \
        : "+r"(dest), "+r"(info) \
        : "r"(scno) \
    ); \
} while(0)


#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV_10(ep, tag, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("r6") = ro; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info) \
        : "r"(scno), "r" (ro_copy) \
        : "r2", "r3", "r4", "r5" \
    ); \
} while(0)

#define DO_REPLY_RECV(ep, tag, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("r6") = ro; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info) \
        : "r"(scno), "r" (ro_copy) \
    ); \
} while(0)

#define DO_RECV(ep, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1"); \
    register seL4_Word scno asm("r7") = seL4_SysRecv; \
    register seL4_Word ro_copy asm("r6") = ro; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "=r"(info) \
        : "r"(scno), "r" (ro_copy) \
    ); \
} while(0)
#else
#define DO_REPLY_RECV_10(ep, tag, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info) \
        : "r"(scno) \
        : "r2", "r3", "r4", "r5" \
    ); \
} while(0)

#define DO_REPLY_RECV(ep, tag, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = tag; \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info) \
        : "r"(scno) \
    ); \
} while(0)

#define DO_RECV(ep, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1"); \
    register seL4_Word scno asm("r7") = seL4_SysRecv; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "=r"(info) \
        : "r"(scno) \
    ); \
} while(0)
#endif /* CONFIG_KERNEL_MCS */

#define DO_REAL_CALL(ep, tag) DO_CALL(ep, tag, "swi #0")
#define DO_NOP_CALL(ep, tag) DO_CALL(ep, tag, "nop")
#define DO_REAL_CALL_10(ep, tag) DO_CALL_10(ep, tag, "swi #0")
#define DO_NOP_CALL_10(ep, tag) DO_CALL_10(ep, tag, "nop")
#define DO_REAL_SEND(ep, tag) DO_SEND(ep, tag, "swi #0")
#define DO_NOP_SEND(ep, tag) DO_SEND(ep, tag, "nop")

#define DO_REAL_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, "swi #0")
#define DO_NOP_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, "nop")
#define DO_REAL_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, "swi #0")
#define DO_NOP_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, "nop")
#define DO_REAL_RECV(ep, ro) DO_RECV(ep, ro, "swi #0")
#define DO_NOP_RECV(ep, ro) DO_RECV(ep, ro, "nop")
