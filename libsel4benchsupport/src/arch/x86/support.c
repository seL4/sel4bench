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
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>
#include <utils/util.h>

#include "../../support.h"

void
benchmark_arch_get_timers(env_t *env)
{
    /* Map the HPET so we can query its proprties */
    vka_object_t frame;
    void *vaddr;
    vaddr = sel4platsupport_map_frame_at(&env->delegate_vka, &env->vspace, env->timer_paddr, seL4_PageBits, &frame);
    int irq;
    int vector;
    ZF_LOGF_IF(vaddr == NULL, "Failed to map HPET paddr");
    if (!hpet_supports_fsb_delivery(vaddr)) {
        if (!config_set(CONFIG_IRQ_IOAPIC)) {
            ZF_LOGF("HPET does not support FSB delivery and we are not using the IOAPIC");
        }
        uint32_t irq_mask = hpet_ioapic_irq_delivery_mask(vaddr);
        /* grab the first irq */
        irq = FFS(irq_mask) - 1;
    } else {
        irq = -1;
    }
    vector = DEFAULT_TIMER_INTERRUPT;
    vspace_unmap_pages(&env->vspace, vaddr, 1, seL4_PageBits, VSPACE_PRESERVE);
    vka_free_object(&env->delegate_vka, &frame);
    env->timeout_timer = sel4platsupport_get_hpet_paddr(&env->vspace, &env->simple, &env->delegate_vka,
                                         env->timer_paddr, env->ntfn.cptr,
                                         irq, vector);
    ZF_LOGF_IF(env->timeout_timer == NULL, "Failed to get HPET timer");
}

static seL4_Error
get_msi(UNUSED void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, TIMEOUT_TIMER_IRQ_SLOT, seL4_WordBits);
    assert(error == seL4_NoError);

    return seL4_NoError;
}

static seL4_Error
get_ioapic(UNUSED void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word ioapic, UNUSED seL4_Word pin, UNUSED seL4_Word edge,
        UNUSED seL4_Word level, seL4_Word vector)
{
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, TIMEOUT_TIMER_IRQ_SLOT, seL4_WordBits);

    assert(error == seL4_NoError);

    return seL4_NoError;
}

void
benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->msi = get_msi;
    simple->ioapic = get_ioapic;
}


