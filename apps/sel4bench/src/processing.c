/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <utils/zf_log.h>
#include "processing.h"
#include "math.h"
#include "printing.h"

int
results_stable(ccnt_t *array, int size)
{
    uint32_t i;
    for (i = 1; i < size; i++) {
        if (array[i] != array[i - 1]) {
            return 0;
        }
    }
    return 1;
}

bench_result_t
process_result(ccnt_t *array, int size, const char *error)
{
    bench_result_t result;

    if (error != NULL && !results_stable(array, size)) {
        ZF_LOGW("%s cycles are not stable\n", error);
        print_all(array, size);
    }

    result.min = results_min(array, size);
    result.max = results_max(array, size);
    result.mean = results_mean(array, size);
    result.variance = results_variance(array, result.mean, size);
    result.stddev = results_stddev(array, result.variance, size);
    result.stddev_pc = (double) result.stddev / (double) result.mean * 100;

    return result;
}

