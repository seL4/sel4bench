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

#include <autoconf.h>
#include <stdio.h>

#include <benchmark.h>
#include <platsupport/plat/timer.h>
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>
#include <utils/util.h>

#include "../../support.h"

static inline sel4ps_irq_t
hpet_irq(env_t *env)
{
    assert(env != NULL);
    assert(env->args != NULL);
    return env->args->to.irqs[0];
}

void
benchmark_arch_get_timers(env_t *env, ps_io_ops_t ops)
{
    /* we know the HPET driver has just one pmem region and one irq -> just grab them */
    ps_irq_t irq = hpet_irq(env).irq;
    pmem_region_t region = env->args->to.objs[0].region;
    int error = ltimer_hpet_init(&env->timer.ltimer, ops, irq, region);
    ZF_LOGF_IF(error, "Failed to get HPET timer");
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    assert(data != NULL);
    env_t *env = data;
    seL4_CPtr irq = sel4platsupport_timer_objs_get_irq_cap(&env->args->to, vector, PS_MSI);

    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, irq, seL4_WordBits);
    assert(error == seL4_NoError);

    return seL4_NoError;
}

static seL4_Error
get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word ioapic, UNUSED seL4_Word pin, UNUSED seL4_Word edge,
        UNUSED seL4_Word level, seL4_Word vector)
{
    assert(data != NULL);
    env_t *env = data;
    seL4_CPtr irq = sel4platsupport_timer_objs_get_irq_cap(&env->args->to, vector, PS_MSI);
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, irq, seL4_WordBits);

    assert(error == seL4_NoError);

    return seL4_NoError;
}

void
benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->msi = get_msi;
    simple->ioapic = get_ioapic;
}
