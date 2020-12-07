/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <autoconf.h>

#ifdef CONFIG_SYSCALL

#define DO_NTFN_OP(ntfn, sys, syscall) do {\
    uint64_t ep_copy = ntfn; \
    asm volatile ( \
            "movq %%rsp, %%rbx \n" \
            sys " \n" \
            "movq %%rbx, %%rsp \n" \
            : "+D" (ep_copy) \
            : "d" ((seL4_Word)syscall) \
            : "%rcx", "%rbx", "%r11" \
    ); \
} while (0);

#ifdef CONFIG_KERNEL_MCS
#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysWait)
#else
#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysRecv)
#endif
#define DO_SIGNAL(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysSend)

#define DO_REAL_WAIT(ntfn)       DO_WAIT(ntfn, "syscall")
#define DO_NOP_WAIT(ntfn)        DO_WAIT(ntfn, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SIGNAL(ntfn)     DO_SIGNAL(ntfn, "syscall")
#define DO_NOP_SIGNAL(ntfn)      DO_SIGNAL(ntfn, ".byte 0x66\n.byte 0x90")

#else
#error Only support benchmarking with syscall as sysenter is known to be slower
#endif /* CONFIG_SYSCALL */
