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

#include <utils/util.h>

#include "../../support.h"

void
benchmark_arch_get_timers(env_t *env, ps_io_ops_t ops)
{
    int error = ltimer_default_init(&env->timer.ltimer, ops);
    ZF_LOGF_IF(error, "Failed to create timeout timer");
}

static seL4_Error get_irq_trigger(void *data, int irq, int trigger, seL4_CNode cnode,
                                  seL4_Word index, uint8_t depth)
{
    env_t *env = data;
    seL4_CPtr cap = sel4platsupport_timer_objs_get_irq_cap(&env->args->to, irq, PS_TRIGGER);
    cspacepath_t path;
    vka_cspace_make_path(&env->slab_vka, cap, &path);
    seL4_Error error = seL4_CNode_Move(cnode, index, depth,
                                       path.root, path.capPtr, path.capDepth);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to move irq cap");

    return seL4_NoError;
}

void benchmark_arch_get_simple(arch_simple_t *simple)
{
    simple->irq_trigger = get_irq_trigger;
}
