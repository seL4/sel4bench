/*
 * Copyright 2020, Data61
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

#define DO_NULLSYSCALL_OP(swi, syscall) do {\
    register seL4_Word scno asm("a7") = syscall; \
    asm volatile (NOPS swi NOPS :: "r"(scno)); \
} while (0);

#define DO_NULLSYSCALL(swi) DO_NULLSYSCALL_OP(swi, seL4_SysBenchmarkNullSyscall)

#define DO_REAL_NULLSYSCALL()     DO_NULLSYSCALL("ecall")
#define DO_NOP_NULLSYSCALL()      DO_NULLSYSCALL("nop")