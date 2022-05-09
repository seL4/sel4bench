/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "benchmark.h"

result_t calculate_results(const size_t n, ccnt_t data[n]);

/*
 * The function calculates parameters of array elements received from
 * a benchmark which used early processing methodology
 * @param num - number of samples
 * @param sum - sum of samples
 * @param sum2 - sum of squared samples
 * @param array - array of raw data which are zeros for Early Processing methodology
 * but the array is required for results output function
 */
result_t calculate_results_early_proc(ccnt_t num, ccnt_t sum, ccnt_t sum2,
                                      ccnt_t array[num]);

/* The function calculates variance using sum, sum of squared values and mean
 * @param num - number of samples
 * @param sum - sum of samples
 * @param sum2 - sum of squared samples
 * @param mean - mean of the samples
*/
static double results_variance_early_proc(const size_t num, const ccnt_t sum,
                                          const ccnt_t sum2, const ccnt_t mean);
