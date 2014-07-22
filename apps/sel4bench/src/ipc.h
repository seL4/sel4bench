/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __IPC_NEW_H__
#define __IPC_NEW_H__

#include <sel4/sel4.h>
#include "mem.h"
#include "helpers.h"
#include "timing.h"
#include "test.h"

void ipc_benchmarks_new(struct env* env, const timing_print_mode_t print_mode);

#endif
