/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <stdio.h>
#include <sel4rpc/client.h>
#include <rpc.pb.h>

#include <benchmark.h>

#include <utils/util.h>

#include "../../support.h"

static seL4_Error get_irq_trigger(void *data, int irq, int trigger, int core, seL4_CNode cnode,
                                  seL4_Word index, uint8_t depth)
{
    env_t *env = data;

    RpcMessage msg = {
        .which_msg = RpcMessage_irq_tag,
        .msg.irq = {
            .which_type = IrqAllocMessage_simple_tag,
            .type.simple = {
                .setTrigger = true,
                .irq = irq,
                .trigger = trigger,
            },
        },
    };

    int ret = sel4rpc_call(&env->rpc_client, &msg, cnode, index, depth);
    if (ret) {
        return ret;
    }

    return msg.msg.ret.errorCode;
}

void benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->irq_trigger = get_irq_trigger;
}
