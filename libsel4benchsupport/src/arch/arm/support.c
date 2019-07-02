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
#include <sel4rpc/client.h>
#include <rpc.pb.h>

#include <benchmark.h>

#include <utils/util.h>

#include "../../support.h"

static seL4_Error get_irq_trigger(void *data, int irq, int trigger, seL4_CNode cnode,
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
