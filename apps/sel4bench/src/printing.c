/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <sel4bench/sel4bench.h>
#include <jansson.h>

#include "benchmark.h"
#include "printing.h"
#include "ipc.h"

void print_all(int size, ccnt_t array[size])
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        printf("\t"CCNT_FORMAT"\n", array[i]);
    }
}
