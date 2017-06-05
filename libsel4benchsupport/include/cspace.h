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
#ifndef __CSPACE_H
#define __CSPACE_H

#include <sel4utils/process.h>

typedef enum {
    TIMEOUT_TIMER_IRQ_SLOT = SEL4UTILS_FIRST_FREE,
    UNTYPED_SLOT,
    TIMER_UNTYPED_SLOT,
    /* define new slots here */
    FREE_SLOT /* first free slot in a benchmark's cspace */
} benchmark_cspace_t;

#endif /* __CSPACE_H__ */
