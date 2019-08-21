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

#ifdef CONFIG_SYSCALL

#define DO_CALL(ep, tag, sys) do { \
    uint64_t ep_copy = ep; \
    asm volatile(\
            "movq   %%rsp, %%rbx \n" \
            sys " \n" \
            "movq  %%rbx, %%rsp \n" \
            : \
            "+S" (tag), \
            "+D" (ep_copy) \
            : \
            "d" ((seL4_Word)seL4_SysCall) \
            : \
            "rcx", "rbx", "r11"\
            ); \
} while (0)

#define DO_CALL_10(ep, tag, sys) do {\
    uint64_t ep_copy = ep; \
    asm volatile(                           \
            "movq   %%rsp, %%rbx \n"        \
            sys " \n"                        \
            "movq   %%rbx, %%rsp \n"        \
            :                               \
            "+S" (tag),                     \
            "+D" (ep_copy)                 \
            :                               \
            "d" ((seL4_Word)seL4_SysCall)              \
            :                               \
            "rcx","rbx","r11","r10","r8", "r9", "r15" \
            );                              \
} while (0)

#define DO_SEND(ep, tag, sys) do { \
    uint64_t ep_copy = ep; \
    uint64_t tag_copy = tag.words[0]; \
    asm volatile( \
            "movq   %%rsp, %%rbx \n" \
            sys "\n" \
            "movq   %%rbx, %%rsp \n" \
            : \
            "+S" (tag_copy), \
            "+D" (ep_copy) \
            : \
            "d" ((seL4_Word)seL4_SysSend) \
            : \
            "rcx", "rbx","r11" \
            ); \
} while (0)

#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV(ep, tag, ro, sys) do { \
    uint64_t ep_copy = ep; \
    register seL4_Word ro_copy asm("r12") = ro;\
    asm volatile( \
            "movq   %%rsp, %%rbx \n" \
            sys "\n" \
            "movq  %%rbx, %%rsp \n" \
            : \
            "+S" (tag), \
            "+D" (ep_copy) \
            : \
            "d"((seL4_Word)seL4_SysReplyRecv), \
            "r" (ro_copy) \
            : \
            "rcx", "rbx","r11" \
            ); \
} while (0)

#define DO_REPLY_RECV_10(ep, tag, ro, sys) do { \
    uint64_t ep_copy = ep;                      \
    register seL4_Word ro_copy asm("r12") = ro;\
    asm volatile(                               \
            "movq   %%rsp, %%rbx \n"            \
            sys" \n"                            \
            "movq   %%rbx, %%rsp \n"            \
            :                                   \
            "+S" (tag),                         \
            "+D" (ep_copy)                     \
            :                                   \
            "d" ((seL4_Word)seL4_SysReplyRecv), \
            "r" (ro_copy) \
            :                                   \
            "rcx","rbx","r11","r10","r8", "r9", "r15"  \
            );                                  \
} while (0)

#define DO_RECV(ep, ro, sys) do { \
    uint64_t ep_copy = ep; \
    uint64_t tag = 0; \
    register seL4_Word ro_copy asm("r12") = ro;\
    asm volatile( \
            "movq   %%rsp, %%rbx \n" \
            sys" \n" \
            "movq   %%rbx, %%rsp \n" \
            : \
            "+S" (tag) ,\
            "+D" (ep_copy) \
            : \
            "d" ((seL4_Word)seL4_SysRecv), \
            "r" (ro_copy) \
            :  "rcx", "rbx","r11" \
            ); \
} while (0)
#else
#define DO_REPLY_RECV(ep, tag, ro, sys) do { \
    uint64_t ep_copy = ep; \
    asm volatile( \
            "movq   %%rsp, %%rbx \n" \
            sys "\n" \
            "movq  %%rbx, %%rsp \n" \
            : \
            "+S" (tag), \
            "+D" (ep_copy) \
            : \
            "d"((seL4_Word)seL4_SysReplyRecv) \
            : \
            "rcx", "rbx","r11" \
            ); \
} while (0)

#define DO_REPLY_RECV_10(ep, tag, ro, sys) do { \
    uint64_t ep_copy = ep;                      \
    asm volatile(                               \
            "movq   %%rsp, %%rbx \n"            \
            sys" \n"                            \
            "movq   %%rbx, %%rsp \n"            \
            :                                   \
            "+S" (tag),                         \
            "+D" (ep_copy)                     \
            :                                   \
            "d" ((seL4_Word)seL4_SysReplyRecv)             \
            :                                   \
            "rcx","rbx","r11","r10","r8", "r9", "r15"  \
            );                                  \
} while (0)

#define DO_RECV(ep, ro, sys) do { \
    uint64_t ep_copy = ep; \
    uint64_t tag = 0; \
    asm volatile( \
            "movq   %%rsp, %%rbx \n" \
            sys" \n" \
            "movq   %%rbx, %%rsp \n" \
            : \
            "+S" (tag) ,\
            "+D" (ep_copy) \
            : \
            "d" ((seL4_Word)seL4_SysRecv) \
            :  "rcx", "rbx","r11" \
            ); \
} while (0)
#endif /* CONFIG_KERNEL_MCS */

#define READ_COUNTER_BEFORE(var) do { \
    uint32_t low, high; \
    asm volatile( \
            "cpuid \n" \
            "rdtsc \n" \
            "movl %%edx, %0 \n" \
            "movl %%eax, %1 \n" \
            : "=r"(high), "=r"(low) \
            : \
            : "%rax", "%rbx", "%rcx", "%rdx"); \
    (var) = (((uint64_t)high) << 32ull) | ((uint64_t)low); \
} while (0)

#define READ_COUNTER_AFTER(var) do { \
    uint32_t low, high; \
    asm volatile( \
            "rdtscp \n" \
            "movl %%edx, %0 \n" \
            "movl %%eax, %1 \n" \
            "cpuid \n" \
            : "=r"(high), "=r"(low) \
            : \
            : "%rax", "rbx", "%rcx", "%rdx"); \
    (var) = (((uint64_t)high) << 32ull) | ((uint64_t)low); \
} while (0)

#define DO_REAL_CALL(ep, tag) DO_CALL(ep, tag, "syscall")
#define DO_NOP_CALL(ep, tag) DO_CALL(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_CALL_10(ep, tag) DO_CALL_10(ep, tag, "syscall")
#define DO_NOP_CALL_10(ep, tag) DO_CALL_10(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SEND(ep, tag) DO_SEND(ep, tag, "syscall")
#define DO_NOP_SEND(ep, tag) DO_SEND(ep, tag, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, "syscall")
#define DO_NOP_REPLY_RECV(ep, tag, ro) DO_REPLY_RECV(ep, tag, ro, ".byte 0x66\n.byte 0x90")
#define DO_REAL_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, "syscall")
#define DO_NOP_REPLY_RECV_10(ep, tag, ro) DO_REPLY_RECV_10(ep, tag, ro, ".byte 0x66\n.byte 0x90")
#define DO_REAL_RECV(ep, ro) DO_RECV(ep, ro, "syscall")
#define DO_NOP_RECV(ep, ro) DO_RECV(ep, ro, ".byte 0x66\n.byte 0x90")

#else
#error Only support benchmarking with syscall as sysenter is known to be slower
#endif /* CONFIG_SYSCALL */
