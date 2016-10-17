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

#include <autoconf.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>

#include <benchmark.h>
#include <sync.h>

#define NOPS ""

#define N_LO_SIGNAL_ARGS 4
#define N_HI_SIGNAL_ARGS 3
#define N_WAIT_ARGS 3

void
abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void
__arch_putchar(int c)
{
    benchmark_putchar(c);
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    vka_object_t done_ep, ntfn;
    sync_results_t *results;

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

