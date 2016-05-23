/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSDv2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <stdio.h>

#include <benchmark.h>
#include <sel4platsupport/plat/timer.h>
#include <utils/util.h>

#include "../../support.h"

void
benchmark_arch_get_timers(env_t *env)
{
    env->timeout_timer = sel4platsupport_get_default_timer(&env->vka, &env->vspace, &env->simple,
                                                           env->ntfn.cptr);
    ZF_LOGF_IF(env->timeout_timer == NULL, "Failed to create timeout timer");
}

static seL4_Error
get_msi(UNUSED void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector) 
{
    assert(vector == (DEFAULT_TIMER_INTERRUPT + IRQ_OFFSET));
    
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, TIMEOUT_TIMER_IRQ_SLOT, seL4_WordBits);
    assert(error == seL4_NoError);

    return seL4_NoError;
}

static seL4_CPtr
get_port(void *data, uint16_t start_port, uint16_t end_port)
{
    return TIMEOUT_TIMER_FRAME_SLOT;
}

void
benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->msi = get_msi;
    simple->IOPort_cap = get_port;
}


