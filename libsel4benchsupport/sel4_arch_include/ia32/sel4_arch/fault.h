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
#include <sel4bench/arch/sel4bench.h>

#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV_1(ep, msg0, ro, sys) do { \
    uint32_t ep_copy = ep; \
    uint32_t ro_copy = ro; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1); \
    asm volatile( \
        "pushl %%ebp \n"\
        "movl %%ecx, %%ebp \n"\
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        "popl %%ebp \n"\
        : \
         "+S" (tag), \
         "+b" (ep_copy), \
         "=D" (msg0), \
         "+c" (ro_copy) \
        : \
         "a" (seL4_SysReplyRecv), \
         "D" (msg0) \
        : \
         "edx" \
    ); \
} while(0)

static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, reply);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_SendWithMRs(dest, seL4_MessageInfo_new(0, 0, 0, 1), &mr0);
}

#else

#define DO_REPLY_RECV_1(ep, msg0, ro, sys) do { \
    uint32_t ep_copy = ep; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1); \
    asm volatile( \
        "pushl %%ebp \n"\
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        "popl %%ebp \n"\
        : \
         "+S" (tag), \
         "+b" (ep_copy), \
         "=D" (msg0) \
        : \
         "a" (seL4_SysReplyRecv), \
         "D" (msg0) \
        : \
         "ecx", \
         "edx" \
    ); \
} while(0)


static inline seL4_MessageInfo_t seL4_RecvWith1MR(seL4_CPtr src, seL4_Word *mr0, UNUSED seL4_CPtr reply)
{
    return seL4_RecvWithMRs(src, NULL, mr0, NULL);
}

static inline void seL4_ReplyWith1MR(seL4_Word mr0, UNUSED seL4_CPtr dest)
{
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, 1), &mr0, NULL);
}
#endif /* CONFIG_KERNEL_MCS */

#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "sysenter")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, ".byte 0x66\n.byte 0x90")
