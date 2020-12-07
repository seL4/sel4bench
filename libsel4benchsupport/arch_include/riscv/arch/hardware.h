/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#define DO_NULLSYSCALL_OP(swi, syscall) do {\
    register seL4_Word scno asm("a7") = syscall; \
    asm volatile (NOPS swi NOPS :: "r"(scno)); \
} while (0);

#define DO_NULLSYSCALL(swi) DO_NULLSYSCALL_OP(swi, seL4_SysBenchmarkNullSyscall)

#define DO_REAL_NULLSYSCALL()     DO_NULLSYSCALL("ecall")
#define DO_NOP_NULLSYSCALL()      DO_NULLSYSCALL("nop")