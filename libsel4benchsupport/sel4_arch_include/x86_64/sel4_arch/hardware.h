/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#define DO_NULLSYSCALL_OP(sys, syscall) do {\
    asm volatile ( \
            "movq %%rsp, %%rbx \n" \
            sys " \n" \
            "movq %%rbx, %%rsp \n" \
            : \
            : "d" ((seL4_Word)syscall) \
            : "%rcx", "%rbx", "%r11" \
    ); \
} while (0);

#define DO_NULLSYSCALL(swi) DO_NULLSYSCALL_OP(swi, seL4_SysBenchmarkNullSyscall)

#define DO_REAL_NULLSYSCALL()     DO_NULLSYSCALL("syscall")
#define DO_NOP_NULLSYSCALL()      DO_NULLSYSCALL(".byte 0x66\n.byte 0x90")
