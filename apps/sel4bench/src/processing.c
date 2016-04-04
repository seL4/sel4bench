/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#include <utils/zf_log.h>
#include <utils/config.h>

#include "ipc.h"
#include "processing.h"
#include "math.h"
#include "printing.h"

result_t
process_result(ccnt_t *array, int size, const char *error)
{
    result_t result;

    if (error != NULL && !results_stable(array, size)) {
        ZF_LOGW("%s cycles are not stable\n", error); 
        if (ZF_LOG_LEVEL <= ZF_LOG_VERBOSE) {
            print_all(array, size);
        }
    }

    result.min = results_min(array, size);
    result.max = results_max(array, size);
    result.mean = results_mean(array, size);
    result.variance = results_variance(array, result.mean, size);
    result.stddev = results_stddev(array, result.variance, size);
    result.stddev_pc = (double) result.stddev / (double) result.mean * 100;

    return result;
}

