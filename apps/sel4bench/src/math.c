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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "benchmark.h"
#include "math.h"

/* these functions adapted from libgsl -- require code to be GPL */
static double
results_mean(const size_t n, const ccnt_t array[n])
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

static double
results_variance(const size_t n, const ccnt_t array[n], const ccnt_t mean)
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

static double
results_stddev(const size_t n, const ccnt_t array[n], const long double variance)
{
    return sqrt ( variance * ((double) n / (double) (n - 1.0f)));
}

static double
results_median(const size_t n, const ccnt_t sorted_data[n])
{
    double median;
    const size_t lhs = (n - 1) / 2;
    const size_t rhs = n / 2;
    size_t stride = 1;

    if (n == 0) {
        return 0.0;
    }

    if (lhs == rhs) {
        median = sorted_data[lhs * stride];
    } else {
        median = (sorted_data[lhs * stride] + sorted_data[rhs * stride]) / 2.0;
    }

    return median;
}

static double
results_quantile(const size_t n, const ccnt_t sorted_data[n], const double quantile)
{
    const double index = quantile * (n - 1) ;
    const size_t lhs = (int)index ;
    const double delta = index - lhs ;
    double result;
    const size_t stride = 1;

    if (n == 0) {
        return 0.0;
    }

    if (lhs == n - 1) {
        result = sorted_data[lhs * stride] ;
    } else {
        result = (1 - delta) * sorted_data[lhs * stride] + delta * sorted_data[(lhs + 1) * stride] ;
    }

    return result;
}

static ccnt_t
results_mode(const size_t n, const ccnt_t sorted_data[n])
{
    if (n == 0) {
        return 0;
    } else if (n == 1) {
        return sorted_data[0];
    }

    ccnt_t mode = sorted_data[0];
    ccnt_t current_value = sorted_data[0];
    ccnt_t current_freq = 1;
    ccnt_t mode_freq = 0;

    for (size_t i = 1; i < n; i++) {
        if (current_value == sorted_data[i]) {
            current_freq++;
        } else {
            if (current_freq > mode_freq) {
                mode_freq = current_freq;
                mode = current_value;
            }

            current_value = sorted_data[i];
            current_freq = 1;
        }
    }

    if (mode_freq == 0) {
        mode_freq = current_freq;
    }

    return mode;
}

static int
ccnt_compare_fn(const void *a, const void *b)
{
    ccnt_t first = *((ccnt_t *) a);
    ccnt_t second = *((ccnt_t *) b);

    return (first > second) - (first < second);
}

result_t
calculate_results(const size_t n, ccnt_t data[n])
{
    /* create a copy of the data to sort */
    ccnt_t sorted_data[n];
    memcpy(sorted_data, data, n * sizeof(ccnt_t));

    /* sort the data */
    qsort(sorted_data, n, sizeof(ccnt_t), ccnt_compare_fn);

    result_t result;
    result.min = sorted_data[0];
    result.max = sorted_data[n - 1];
    assert(result.min <= result.max);
    result.mean = results_mean(n, sorted_data);
    result.variance = results_variance(n, sorted_data, result.mean);
    result.stddev = results_stddev(n, sorted_data, result.variance);
    result.median = results_median(n, sorted_data);
    result.first_quantile = results_quantile(n, sorted_data, 0.25f);
    result.third_quantile = results_quantile(n, sorted_data, 0.75f);
    result.mode = results_mode(n, sorted_data);
    result.raw_data = data;
    result.samples = n;

    return result;
}
