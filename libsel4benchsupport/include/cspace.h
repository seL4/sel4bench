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

#define UNTYPED_SLOT   (SEL4UTILS_FIRST_FREE)
#define SCHED_CTRL_SLOT (UNTYPED_SLOT + 1u)
#define IRQ_SLOT       (SCHED_CTRL_SLOT + 1u)
#define FRAME_SLOT     (IRQ_SLOT + 1u)
#define FREE_SLOT      (FRAME_SLOT + 1u)

#endif /* __CSPACE_H__ */
