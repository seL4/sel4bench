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

#define DO_NTFN_OP(ntfn, sys, syscall) do {\
    uint32_t ep_copy = ntfn; \
    asm volatile ( \
            "movl %%esp, %%ecx \n" \
            "leal 1f, %%edx \n" \
            "1: \n" \
            sys" \n" \
            : "+b" (ep_copy) \
            : "a" (syscall) \
            : "ecx", "edx" \
    ); \
} while (0);

#define DO_WAIT(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysRecv)
#define DO_SIGNAL(ntfn, swi) DO_NTFN_OP(ntfn, swi, seL4_SysSend)

#define DO_REAL_WAIT(ntfn)       DO_WAIT(ntfn, "sysenter")
#define DO_NOP_WAIT(ntfn)        DO_WAIT(ntfn, ".byte 0x66\n.byte 0x90")
#define DO_REAL_SIGNAL(ntfn)     DO_SIGNAL(ntfn, "sysenter")
#define DO_NOP_SIGNAL(ntfn)      DO_SIGNAL(ntfn, ".byte 0x66\n.byte 0x90")

#endif /* __ARCH_SIGNAL_H */

