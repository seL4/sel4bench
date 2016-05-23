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

#include <utils/util.h>

#include "../../support.h"

void
benchmark_arch_get_timers(env_t *env)
{
    env->timeout_timer = sel4platsupport_get_default_timer(&env->vka, &env->vspace, &env->simple,
                                                           env->ntfn.cptr);
    ZF_LOGF_IF(env->timeout_timer == NULL, "Failed to create timeout timer");
}

void
benchmark_arch_get_simple(arch_simple_t *simple)
{
    /* nothing to do */
}


