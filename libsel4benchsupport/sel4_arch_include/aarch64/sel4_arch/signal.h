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

#define DO_NTFN_OP(ntfn, swi, syscall) do {\
    register seL4_Word dest asm("x0") = (seL4_Word) ntfn; \
    register seL4_Word scno asm("x7") = syscall; \
    asm volatile (NOPS swi NOPS \
        : /* no outputs */ \
        : "r" (dest), "r"(scno) \
        : /* no clobber */ \
    ); \
} while (0);

#ifdef CONFIG_KERNEL_MCS
#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysWait)
#else
#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysRecv)
#endif
#define DO_SIGNAL(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysSend)

#define DO_REAL_WAIT(ntfn)       DO_WAIT(ntfn, "svc #0")
#define DO_NOP_WAIT(ntfn)        DO_WAIT(ntfn, "nop")
#define DO_REAL_SIGNAL(ntfn)     DO_SIGNAL(ntfn, "svc #0")
#define DO_NOP_SIGNAL(ntfn)      DO_SIGNAL(ntfn, "nop")
