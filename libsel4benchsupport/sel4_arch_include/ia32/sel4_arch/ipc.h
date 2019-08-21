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

#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV(ep, tag, ro, sys) do { \
    uint32_t ep_copy = ep; \
    uint32_t ro_copy = ro; \
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
         "+c" (ro_copy) \
        : \
         "a" (seL4_SysReplyRecv) \
        : \
         "edx" \
    ); \
} while(0)

#define DO_REPLY_RECV_10(ep, tag, ro, sys) do { \
    uint32_t ep_copy = ep; \
    uint32_t ro_copy = ro; \
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
         "+c" (ro_copy) \
        : \
         "a" (seL4_SysReplyRecv) \
        : \
         "edx", \
         "edi" \
    ); \
} while(0)

#define DO_RECV(ep, ro, sys) do { \
    uint32_t ep_copy = ep; \
    uint32_t ro_copy = ro; \
    asm volatile( \
        "pushl %%ebp \n"\
        "movl %%ecx, %%ebp \n"\
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        "popl %%ebp \n"\
        : \
         "+b" (ep_copy), \
         "+c" (ro_copy) \
        : \
         "a" (seL4_SysRecv) \
        : \
         "edx", \
         "esi" \
    ); \
} while(0)
#else
#define DO_REPLY_RECV(ep, tag, ro, sys) do { \
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

#define DO_REPLY_RECV_10(ep, tag, ro, sys) do { \
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

#define DO_RECV(ep, ro, sys) do { \
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

#endif /* CONFIG_KERNEL_MCS */

#define READ_COUNTER_BEFORE SEL4BENCH_READ_CCNT
#define READ_COUNTER_AFTER  SEL4BENCH_READ_CCNT

#define DO_REAL_CALL(ep, tag) DO_CALL(ep, tag, "sysenter")
#define DO_NOP_CALL(ep, tag) DO_CALL(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_CALL_10(ep, tag) DO_CALL_10(ep, tag, "sysenter")
#define DO_NOP_CALL_10(ep, tag) DO_CALL_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SEND(ep, tag) DO_SEND(ep, tag, "sysenter")
#define DO_NOP_SEND(ep, tag) DO_SEND(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, "sysenter")
#define DO_NOP_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, "sysenter")
#define DO_NOP_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, ".byte 0x66\n.byte 0x90")
#define DO_REAL_RECV(ep, ro) DO_RECV(ep, ro, "sysenter")
#define DO_NOP_RECV(ep, ro) DO_RECV(ep, ro, ".byte 0x66\n.byte 0x90")
