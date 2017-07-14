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

/**
 * Process a table of results that need to be divided by AVERAGE_RUNS
 *
 * Each column in the table is a single result type.
 *
 * First divide all the results by AVERAGE_RUNS, then process each column.
 *
 * @param rows the number of rows in the results (should also be number of runs)
 * @param cols the number of columns in the results (also number of result types)
 * @param array raw results
 * @param results destination of the result of this function
 */
void process_average_results(int rows, int cols, ccnt_t array[rows][cols], result_t results[cols]);
