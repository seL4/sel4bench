/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "benchmark.h"
#include "math.h"

/* these functions adapted from libgsl -- require code to be GPL */
double
results_mean(ccnt_t *array, int n)
{
    size_t i;
    long double mean = 0;
    /* Compute the arithmetic mean of a dataset using the recurrence relation
     * mean_(n) = mean(n-1) + (data[n] - mean(n-1))/(n+1)   */

    for (i = 0; i < n; i++) {
        mean += (array[i] - mean) / (i + 1);
    }
    return mean;
}

double
results_variance(ccnt_t *array, ccnt_t mean, int n)
{
    long double variance = 0;

    size_t i;

    /* find the sum of the squares */
    for (i = 0; i < n; i++) {
        const long double delta = (long double) array[i] - (long double) mean;
        variance += (delta * delta - variance) / (i + 1);
    }

    return (double) variance;
}

double
results_stddev(ccnt_t *array, long double variance, int n)
{
    return sqrt ( variance * ((double) n / (double) (n - 1.0f)));
}

ccnt_t
results_max(ccnt_t *array, int n)
{
    size_t i;
    ccnt_t max = array[0];
    for (i = 1; i < n; i++) {
        if (array[i] > max) {
            max = array[i];
        }
    }
    return max;
}

ccnt_t
results_min(ccnt_t *array, int n)
{
    uint32_t i;
    ccnt_t min = array[0];
    for (i = 1; i < n; i++) {
        if (array[i] < min) {
            min = array[i];
        }
    }
    return min;
}

