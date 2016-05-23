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
benchmark_mach_get_clock_timer(env_t *env)
{
    env->clock_timer = sel4platsupport_get_gpt(&env->vspace, &env->simple, &env->vka, 
                                               env->ntfn.cptr, 0);
    ZF_LOGF_IF(env->clock_timer == NULL, "Failed to start timeout timer");
}


