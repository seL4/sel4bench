/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __CSPACE_H
#define __CSPACE_H

#include <sel4utils/process.h>

typedef enum {
    UNTYPED_SLOT = SEL4UTILS_FIRST_FREE,
    SCHED_CTRL_SLOT,
    TIMEOUT_TIMER_IRQ_SLOT,
    TIMEOUT_TIMER_FRAME_SLOT,
    CLOCK_FRAME_SLOT,
    CLOCK_IRQ_SLOT,
    /* define new slots here */
    FREE_SLOT /* first free slot in a benchmark's cspace */
} benchmark_cspace_t;

#endif /* __CSPACE_H__ */
