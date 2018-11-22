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
#pragma once

#include <platsupport/io.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/timer.h>
#include <sel4utils/sel4_zf_logif.h>
#include <simple/simple.h>
#include <vka/vka.h>
#include <vka/object.h>
#include <vspace/vspace.h>
#include <utils/util.h>

/* Contains information about the benchmark environment. */
typedef struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    simple_t simple;
    vspace_t vspace;
    /* regular untyped memory to pass to benchmark apps */
    vka_object_t untyped;
    timer_objects_t to;
    ps_io_ops_t ops;
} env_t;

/* do any platform specific set up */
void plat_setup(env_t *env) WEAK;
int arch_init_timer_irq_cap(env_t *env, cspacepath_t *path);
