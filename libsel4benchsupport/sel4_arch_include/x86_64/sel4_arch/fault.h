/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 23
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#pragma once

#include <autoconf.h>

#ifdef CONFIG_SYSCALL

#include <sel4bench/arch/sel4bench.h>

#define DO_REPLY_RECV_1(ep, msg0, sys) do { \
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
         "%r8" \
    ); \
    msg0 = mr0; \
} while(0)

#define DO_REAL_REPLY_RECV_1(ep, mr0) DO_REPLY_RECV_1(ep, mr0, "syscall")
#define DO_NOP_REPLY_RECV_1(ep, mr0) DO_REPLY_RECV_1(ep, mr0, ".byte 0x66\n.byte 0x90")

static inline seL4_MessageInfo_t
seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0) {
    return seL4_RecvWithMRs(src, NULL, mr0, NULL);
}

static inline void
seL4_ReplyWith1MR(seL4_Word mr0)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL);
}

#else
#error Only support benchmarking with syscall as sysenter is known to be slower
#endif /* CONFIG_SYSCALL */
