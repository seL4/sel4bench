/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __SEL4_ARCH_IPC_H
#define __SEL4_ARCH_IPC_H

#define ALLOW_UNSTABLE_OVERHEAD

#include <sel4bench/arch/sel4bench.h>

#define DO_CALL(ep, tag, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        : \
         "+S" (tag), \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysCall) \
        : \
         "ecx", \
         "edx" \
    ); \
} while(0)

#define DO_CALL_10(ep, tag, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "pushl %%ebp \n"\
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        "popl %%ebp \n"\
        : \
         "+S" (tag), \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysCall) \
        : \
         "ecx", \
         "edx", \
         "edi" \
    ); \
} while(0)

#define DO_SEND(ep, tag, sys) do { \
    uint32_t ep_copy = ep; \
    uint32_t tag_copy = tag.words[0]; \
    asm volatile( \
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        : \
         "+S" (tag_copy), \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysSend) \
        : \
         "ecx", \
         "edx" \
    ); \
} while(0)

#define DO_REPLY_RECV(ep, tag, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        : \
         "+S" (tag), \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysReplyRecv) \
        : \
         "ecx", \
         "edx" \
    ); \
} while(0)

#define DO_REPLY_RECV_10(ep, tag, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "pushl %%ebp \n"\
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        "popl %%ebp \n"\
        : \
         "+S" (tag), \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysReplyRecv) \
        : \
         "ecx", \
         "edx", \
         "edi" \
    ); \
} while(0)

#define DO_RECV(ep, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        : \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysRecv) \
        : \
         "ecx", \
         "edx", \
         "esi" \
    ); \
} while(0)

#define READ_COUNTER_BEFORE SEL4BENCH_READ_CCNT
#define READ_COUNTER_AFTER  SEL4BENCH_READ_CCNT

#define DO_REAL_CALL(ep, tag) DO_CALL(ep, tag, "sysenter")
#define DO_NOP_CALL(ep, tag) DO_CALL(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV(ep, tag) DO_REPLY_RECV(ep, tag, "sysenter")
#define DO_NOP_REPLY_RECV(ep, tag) DO_REPLY_RECV(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_CALL_10(ep, tag) DO_CALL_10(ep, tag, "sysenter")
#define DO_NOP_CALL_10(ep, tag) DO_CALL_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV_10(ep, tag) DO_REPLY_RECV_10(ep, tag, "sysenter")
#define DO_NOP_REPLY_RECV_10(ep, tag) DO_REPLY_RECV_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SEND(ep, tag) DO_SEND(ep, tag, "sysenter")
#define DO_NOP_SEND(ep, tag) DO_SEND(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_RECV(ep) DO_RECV(ep, "sysenter")
#define DO_NOP_RECV(ep) DO_RECV(ep, ".byte 0x66\n.byte 0x90")


#endif /* __SEL4_ARCH_IPC_H */

