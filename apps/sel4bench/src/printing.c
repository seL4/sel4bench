/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <stdio.h>
#include "benchmark.h"

void
print_all(ccnt_t *array, int size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        printf("\t"CCNT_FORMAT"\n", array[i]);
    }
}
