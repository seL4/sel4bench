/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#define DO_NULLSYSCALL_OP(swi, syscall) do {\
    register seL4_Word scno asm("r7") = syscall; \
    asm volatile (NOPS swi NOPS \
        : /* no outputs */ \
        : "r"(scno) \
        : /* no clobber */ \
    ); \
} while (0);

#define DO_REAL_NULLSYSCALL()     DO_NULLSYSCALL("swi #0")
#define DO_NOP_NULLSYSCALL()      DO_NULLSYSCALL("nop")
