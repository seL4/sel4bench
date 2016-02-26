/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef __SEL4BENCH_MATH_H
#define __SEL4BENCH_MATH_H

#include "benchmark.h"

/* these functions adapted from libgsl -- require code to be GPL */
double results_mean(ccnt_t *array, int n);
double results_variance(ccnt_t *array, ccnt_t mean, int n);
double results_stddev(ccnt_t *array, long double variance, int n);
ccnt_t results_max(ccnt_t *array, int n);
ccnt_t results_min(ccnt_t *array, int n);

#endif /* __SEL4BENCH_MATH_H */
