/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef __SEL4_ARCH_HARDWARE_H
#define __SEL4_ARCH_HARDWARE_H

#define DO_NULLSYSCALL_OP(sys, syscall) do {\
    asm volatile ( \
            "movl %%esp, %%ecx \n" \
            "leal 1f, %%edx \n" \
            "1: \n" \
            sys" \n" \
            : \
            : "a" (syscall) \
            : "ecx", "edx" \
    ); \
} while (0);

#define DO_NULLSYSCALL(swi) DO_NULLSYSCALL_OP(swi, seL4_SysBenchmarkNullSyscall)

#define DO_REAL_NULLSYSCALL()     DO_NULLSYSCALL("sysenter")
#define DO_NOP_NULLSYSCALL()      DO_NULLSYSCALL(".byte 0x66\n.byte 0x90")

#endif /* __SEL4_ARCH_HARDWARE_H */
