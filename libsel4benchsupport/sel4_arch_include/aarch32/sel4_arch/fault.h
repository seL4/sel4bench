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

#ifdef CONFIG_KERNEL_MCS
#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word ro_copy asm("r6") = (seL4_Word) ro; \
    register seL4_Word mr0 asm("r2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno), "r" (ro_copy)\
        : "r3", "r4", "r5" \
    ); \
    ip = mr0; \
} while(0)


#else
#define DO_REPLY_RECV_1(ep, ip, ro, swi) do { \
    register seL4_Word src asm("r0") = (seL4_Word)ep; \
    register seL4_MessageInfo_t info asm("r1") = seL4_MessageInfo_new(0, 0, 0, 1); \
    register seL4_Word scno asm("r7") = seL4_SysReplyRecv; \
    register seL4_Word mr0 asm("r2") = ip; \
    asm volatile(NOPS swi NOPS \
        : "+r"(src), "+r"(info), "+r" (mr0) \
        : "r"(scno) \
        : "r3", "r4", "r5" \
    ); \
    ip = mr0; \
} while(0)
#endif /* CONFIG_KERNEL_MCS */

#define DO_REAL_REPLY_RECV_1(ep, mr0, ro) DO_REPLY_RECV_1(ep, mr0, ro, "swi $0")
#define DO_NOP_REPLY_RECV_1(ep, mr0, ro)  DO_REPLY_RECV_1(ep, mr0, ro, "nop")
