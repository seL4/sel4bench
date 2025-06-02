/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <autoconf.h>

#ifdef CONFIG_SYSCALL

#include <sel4bench/arch/sel4bench.h>

#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV_1(ep, msg0, ro, sys) do { \
    uint64_t ep_copy = ep; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word mr0 asm("r10") = msg0; \
    register seL4_Word ro_copy asm("r12") = ro;\
    asm volatile( \
        "movq %%rsp, %%rbx \n" \
        sys" \n" \
        "mov %%rbx, %%rsp \n"\
        : \
         "+S" (tag), \
         "+D" (ep_copy), \
         "=r" (mr0) \
        : \
         "d" ((seL4_Word)seL4_SysReplyRecv), \
         "r" (mr0), \
         "r" (ro_copy)\
        : \
         "%rcx", \
         "%rbx", \
         "%r11", \
         "%r8",  \
         "%r9",  \
         "%r15"  \
    ); \
    msg0 = mr0; \
} while(0)

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, sys) do { \
    uint64_t ep_copy = ep; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0); \
    register seL4_Word mr0 asm("r10"); \
    register seL4_Word mr1 asm("r8"); \
    register seL4_Word mr2 asm("r9"); \
    register seL4_Word mr3 asm("r15"); \
    register seL4_Word ro_copy asm("r12") = ro;\
    asm volatile( \
        "movq %%rsp, %%rbx \n" \
        sys" \n" \
        "mov %%rbx, %%rsp \n"\
        : \
         "+S" (tag), \
         "+D" (ep_copy), \
         "=r" (mr0), \
         "=r" (mr1), \
         "=r" (mr2), \
         "=r" (mr3) \
        : \
         "d" ((seL4_Word)seL4_SysReplyRecv), \
         "r" (ro_copy)\
        : \
         "%rcx", \
         "%rbx", \
         "%r11" \
    ); \
    m0 = mr0; \
    m1 = mr1; \
    m2 = mr2; \
    m3 = mr3; \
} while(0)

static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL, reply);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, seL4_CPtr dest)
{
    return seL4_SendWithMRs(dest, seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}

#else
#define DO_REPLY_RECV_1(ep, msg0, ro, sys) do { \
    uint64_t ep_copy = ep; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word mr0 asm("r10") = msg0; \
    asm volatile( \
        "movq %%rsp, %%rbx \n" \
        sys" \n" \
        "mov %%rbx, %%rsp \n"\
        : \
         "+S" (tag), \
         "+D" (ep_copy), \
         "=r" (mr0) \
        : \
         "d" ((seL4_Word)seL4_SysReplyRecv), \
         "r" (mr0) \
        : \
         "%rcx", \
         "%rbx", \
         "%r11", \
         "%r8",  \
         "%r9",  \
         "%r15"  \
    ); \
    msg0 = mr0; \
} while(0)

#define DO_REPLY_0_RECV_4(ep, m0, m1, m2, m3, ro, sys) do { \
    uint64_t ep_copy = ep; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0); \
    register seL4_Word mr0 asm("r10"); \
    register seL4_Word mr1 asm("r8"); \
    register seL4_Word mr2 asm("r9"); \
    register seL4_Word mr3 asm("r15"); \
    asm volatile( \
        "movq %%rsp, %%rbx \n" \
        sys" \n" \
        "mov %%rbx, %%rsp \n"\
        : \
         "+S" (tag), \
         "+D" (ep_copy), \
         "=r" (mr0), \
         "=r" (mr1), \
         "=r" (mr2), \
         "=r" (mr3) \
        : \
         "d" ((seL4_Word)seL4_SysReplyRecv) \
        : \
         "%rcx", \
         "%rbx", \
         "%r11" \
    ); \
    m0 = mr0; \
    m1 = mr1; \
    m2 = mr2; \
    m3 = mr3; \
} while(0)

static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, UNUSED seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, NULL, NULL, NULL);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL, NULL, NULL);
}
#endif /* CONFIG_KERNEL_MCS */


#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "syscall")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, "syscall")
#define DO_NOP_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro) DO_REPLY_0_RECV_4(ep, mr0, mr1, mr2, mr3, ro, ".byte 0x66\n.byte 0x90")

#else
#error Only support benchmarking with syscall as sysenter is known to be slower
#endif /* CONFIG_SYSCALL */
