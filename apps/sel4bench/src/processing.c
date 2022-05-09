/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <benchmark.h>
#include <utils/zf_log.h>
#include <utils/config.h>

#include "ipc.h"
#include "processing.h"
#include "math.h"
#include "printing.h"

void process_average_results(int rows, int cols, ccnt_t array[rows][cols], result_t results[cols])
{
    /* first divide results by no of runs */
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            array[row][col] /= AVERAGE_RUNS;
        }
    }

    /* now calculate */
    for (int col = 0; col < cols; col++) {
        /* create an array of the specific column we want to process - we can't reorganise the data structure as
         * the 2D array is arranged to minimise benchmark impact such that we write to sequential memory addresses in each loop of the benchmark,
         * additionally we calloc the copy such that the raw data pointed to by the result does not exist on the stack. A different approach will be
         * required if we run out of memory */
        ccnt_t *raw_data = calloc(rows, sizeof(ccnt_t));
        assert(raw_data != NULL);

        for (int i = 0; i < rows; i++) {
            raw_data[i] = array[i][col];
        }

        results[col] = calculate_results(rows, raw_data);
    }
}

result_t process_result(size_t n, ccnt_t array[n], result_desc_t desc)
{
    array = &array[desc.ignored];
    int size = n - desc.ignored;

    if (desc.stable && !results_stable(array, size)) {
        ZF_LOGW("%s cycles are not stable\n", desc.name == NULL ? "unknown" : desc.name);
        if (ZF_LOG_LEVEL <= ZF_LOG_VERBOSE) {
            print_all(size, array);
        }
        if (!config_set(CONFIG_ALLOW_UNSTABLE_OVERHEAD)) {
            return (result_t) {
                0
            };
        }
    }

    for (int i = 0; i < size; i++) {
        array[i] -= desc.overhead;
    }

    return calculate_results(size, array);
}

result_t process_result_early_proc(ccnt_t num, ccnt_t sum, ccnt_t sum2, ccnt_t array[num])
{
    return calculate_results_early_proc(num, sum, sum2, array);
}

void process_results(size_t ncols, size_t nrows, ccnt_t array[ncols][nrows], result_desc_t desc,
                     result_t results[ncols])
{
    for (int i = 0; i < ncols; i++) {
        results[i] = process_result(nrows, array[i], desc);
    }
}
