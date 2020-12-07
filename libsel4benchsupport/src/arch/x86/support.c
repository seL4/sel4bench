/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <stdio.h>

#include <benchmark.h>
#include <platsupport/plat/timer.h>
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>
#include <utils/util.h>
#include <sel4rpc/client.h>
#include <rpc.pb.h>

#include "../../support.h"

static seL4_Error get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
                          UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
                          UNUSED seL4_Word handle, seL4_Word vector)
{
    assert(data != NULL);
    env_t *env = data;

    RpcMessage msg = {
        .which_msg = RpcMessage_irq_tag,
        .msg.irq = {
            .which_type = IrqAllocMessage_msi_tag,
            .type.msi = {
                .pci_bus = pci_bus,
                .pci_dev = pci_dev,
                .pci_func = pci_func,
                .handle = handle,
                .vector = vector,
            },
        },
    };

    int ret = sel4rpc_call(&env->rpc_client, &msg, root, index, depth);
    if (ret) {
        return ret;
    }

    assert(msg.msg.ret.errorCode == seL4_NoError);

    return seL4_NoError;
}

static seL4_Error get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
                             UNUSED seL4_Word ioapic, UNUSED seL4_Word pin, UNUSED seL4_Word edge,
                             UNUSED seL4_Word level, seL4_Word vector)
{
    assert(data != NULL);
    env_t *env = data;

    RpcMessage msg = {
        .which_msg = RpcMessage_irq_tag,
        .msg.irq = {
            .which_type = IrqAllocMessage_ioapic_tag,
            .type.ioapic = {
                .ioapic = ioapic,
                .pin = pin,
                .polarity = edge,
                .level = level,
                .vector = vector,
            },
        },
    };

    int ret = sel4rpc_call(&env->rpc_client, &msg, root, index, depth);
    if (ret) {
        return ret;
    }

    assert(msg.msg.ret.errorCode == seL4_NoError);

    return seL4_NoError;
}

void benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->msi = get_msi;
    simple->ioapic = get_ioapic;
}
