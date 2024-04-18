/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <autoconf.h>

#define DO_NTFN_OP(ntfn, swi, syscall) do {\
    register seL4_Word dest asm("r0") = (seL4_Word) ntfn; \
    register seL4_Word scno asm("r7") = syscall; \
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

#define DO_REAL_WAIT(ntfn)       DO_WAIT(ntfn, "swi #0")
#define DO_NOP_WAIT(ntfn)        DO_WAIT(ntfn, "nop")
#define DO_REAL_SIGNAL(ntfn)     DO_SIGNAL(ntfn, "swi #0")
#define DO_NOP_SIGNAL(ntfn)      DO_SIGNAL(ntfn, "nop")
