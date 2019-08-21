/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <autoconf.h>

static inline void smp_benchmark_ping(seL4_CPtr ep)
{
    seL4_CallWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL);
}

static inline void smp_benchmark_pong(seL4_CPtr ep, seL4_CPtr reply)
{
#ifdef CONFIG_KERNEL_MCS
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, reply);
#else
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL);
#endif
}
