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
#ifndef __SEL4_ARCH_HARDWARE_H
#define __SEL4_ARCH_HARDWARE_H

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

#endif /* __SEL4_ARCH_HARDWARE_H */
