/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#pragma once

#include <platsupport/io.h>
#include <sel4platsupport/io.h>
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
    /* untyped memory for benchmark timer paddr */
    vka_object_t timer_untyped;
    vka_object_t clock_untyped;
    /* physical address of the timeout timer device */
    uintptr_t timer_paddr;
    ps_io_ops_t ops;
} env_t;

/* do any platform specific set up */
void plat_setup(env_t *env);
