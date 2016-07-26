/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#pragma once

#include <sel4/sel4.h>

static inline void
seL4_TimeoutReplyWithMRs(seL4_UserContext regs) {
    return seL4_ReplyWithMRs(seL4_MessageInfo_new(0, 0, 0, sizeof(seL4_UserContext) / sizeof (seL4_Word)),
            &regs.pc, &regs.sp, &regs.cpsr, &regs.r0);
}

static inline void
seL4_RecvNoMRs(seL4_CPtr cap)
{
    seL4_RecvWithMRs(cap, NULL, NULL, NULL, NULL, NULL);
}
