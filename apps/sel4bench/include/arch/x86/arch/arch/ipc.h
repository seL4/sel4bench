/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef __ARCH_IPC_H
#define __ARCH_IPC_H

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

#define DO_REPLY_WAIT(ep, tag, sys) do { \
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
         "a" (seL4_SysReplyWait) \
        : \
         "ecx", \
         "edx" \
    ); \
} while(0)

#define DO_REPLY_WAIT_10(ep, tag, sys) do { \
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
         "a" (seL4_SysReplyWait) \
        : \
         "ecx", \
         "edx", \
         "edi" \
    ); \
} while(0)

#define DO_WAIT(ep, sys) do { \
    uint32_t ep_copy = ep; \
    asm volatile( \
        "movl %%esp, %%ecx \n"\
        "leal 1f, %%edx \n"\
        "1: \n" \
        sys" \n" \
        : \
         "+b" (ep_copy) \
        : \
         "a" (seL4_SysWait) \
        : \
         "ecx", \
         "edx", \
         "esi" \
    ); \
} while(0)

#define READ_COUNTER(var) do { \
    uint32_t low, high; \
    asm volatile( \
        "movl $0, %%eax \n" \
        "movl $0, %%ecx \n" \
        "cpuid \n" \
        "rdtsc \n" \
        "movl %%edx, %0 \n" \
        "movl %%eax, %1 \n" \
        "movl $0, %%eax \n" \
        "movl $0, %%ecx \n" \
        "cpuid \n" \
        : \
         "=r"(high), \
         "=r"(low) \
        : \
        : "eax", "ebx", "ecx", "edx" \
    ); \
    (var) = (((uint64_t)high) << 32ull) | ((uint64_t)low); \
} while(0)

#define READ_COUNTER_BEFORE READ_COUNTER
#define READ_COUNTER_AFTER  READ_COUNTER

#define DO_REAL_CALL(ep, tag) DO_CALL(ep, tag, "sysenter")
#define DO_NOP_CALL(ep, tag) DO_CALL(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_WAIT(ep, tag) DO_REPLY_WAIT(ep, tag, "sysenter")
#define DO_NOP_REPLY_WAIT(ep, tag) DO_REPLY_WAIT(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_CALL_10(ep, tag) DO_CALL_10(ep, tag, "sysenter")
#define DO_NOP_CALL_10(ep, tag) DO_CALL_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_WAIT_10(ep, tag) DO_REPLY_WAIT_10(ep, tag, "sysenter")
#define DO_NOP_REPLY_WAIT_10(ep, tag) DO_REPLY_WAIT_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SEND(ep, tag) DO_SEND(ep, tag, "sysenter")
#define DO_NOP_SEND(ep, tag) DO_SEND(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_WAIT(ep) DO_WAIT(ep, "sysenter")
#define DO_NOP_WAIT(ep) DO_WAIT(ep, ".byte 0x66\n.byte 0x90")


#endif /* __ARCH_IPC_H */

