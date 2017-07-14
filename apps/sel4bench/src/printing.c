/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#include <stdio.h>
#include <sel4bench/sel4bench.h>
#include <jansson.h>

#include "benchmark.h"
#include "printing.h"
#include "ipc.h"

void
print_all(int size, ccnt_t array[size])
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        printf("\t"CCNT_FORMAT"\n", array[i]);
    }
}
