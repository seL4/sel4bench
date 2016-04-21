/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <stdio.h>
#include <sel4bench/sel4bench.h>

#include "benchmark.h"
#include "printing.h"
#include "ipc.h"

void 
print_banner(char *name, int samples)
{
    printf("----------------------------------------\n");
    printf("%s (%d samples)\n", name, samples);
    printf("----------------------------------------\n");
}

void
print_all(ccnt_t *array, int size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        printf("\t"CCNT_FORMAT"\n", array[i]);
    }
}

void
print_result_header(void)
{
    printf("min\tmax\tmean\tstddev %%\n");
            
            //variance\tstddev\tstddev\n");
}

void
print_result(result_t *result)
{
    printf(CCNT_FORMAT"\t", result->min);
    printf(CCNT_FORMAT"\t", result->max);
    printf("%.0lf%%\n", result->stddev_pc);
 //   printf("%.2lf\t", result->mean);
 //   printf("%.2lf\t", result->variance);
 //   printf("%.2lf\n", result->stddev);
}

