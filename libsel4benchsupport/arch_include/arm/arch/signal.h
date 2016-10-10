/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __ARCH_SIGNAL_H
#define __ARCH_SIGNAL_H

#define DO_NTFN_OP(ntfn, swi, syscall) do {\
    register seL4_Word dest asm("r0") = (seL4_Word) ntfn; \
    register seL4_Word scno asm("r7") = syscall; \
    asm volatile (NOPS swi NOPS \
        : /* no outputs */ \
        : "r" (dest), "r"(scno) \
        : /* no clobber */ \
    ); \
} while (0);

#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysRecv)
#define DO_SIGNAL(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysSend)

#define DO_REAL_WAIT(ntfn)       DO_WAIT(ntfn, "swi $0")
#define DO_NOP_WAIT(ntfn)        DO_WAIT(ntfn, "nop")
#define DO_REAL_SIGNAL(ntfn)     DO_SIGNAL(ntfn, "swi $0")
#define DO_NOP_SIGNAL(ntfn)      DO_SIGNAL(ntfn, "nop")

#endif /* __ARCH_SIGNAL_H */

