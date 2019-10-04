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

#include <stdint.h>
#include <sel4/types.h>
#include <sel4platsupport/timer.h>
/* types shared between sel4bench and its child apps */

#define SEL4BENCH_PROTOBUF_RPC (9000)
typedef struct {
    size_t untyped_size_bits;
    uintptr_t stack_vaddr;
    size_t stack_pages;
    void *results;
    int nr_cores;
    void *fdt;
    seL4_CPtr first_free;
    seL4_CPtr untyped_cptr;
    seL4_CPtr sched_ctrl;
    seL4_CPtr serial_ep;
} benchmark_args_t;
