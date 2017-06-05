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

#include <sel4bench/arch/sel4bench.h>

#define DO_REPLY_RECV_1(ep, msg0, sys) do { \
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

#define DO_REAL_REPLY_RECV_1(ep, mr0) DO_REPLY_RECV_1(ep, mr0, "sysenter")
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
