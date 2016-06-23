/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#pragma once

#include "benchmark.h"

/*
 * Compute the variance, standard deviation, mean, min and max for a set of values.
 *
 * @param n     number of values to process
 * @param array values to compute results for.
 * @param desc  further (optional) details about the results
 */
result_t process_result(size_t n, ccnt_t array[n], result_desc_t desc);

/**
 * @param ncols    size of the 1st dimension of array.
 * @param nrows    size of the 2nd dimension of the array.
 * @param array    2d array of results to be processed. The assumption is that array[i] will yield a
 *                 set of results for a specific measurement, i.e array[i][j] and array[i][j+1] are
 *                 for the same result type.
 * @param desc     further (optional) details about the results.
 * @param results  array for output, must be ncols in size.
 */
void process_results(size_t ncols, size_t nrows, ccnt_t array[ncols][nrows], result_desc_t desc,
                     result_t results[ncols]);

