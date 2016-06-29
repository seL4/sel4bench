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
process_result(size_t n, ccnt_t array[n], result_desc_t desc)
{
    array = &array[desc.ignored];
    int size = n - desc.ignored;

    if (desc.stable && !results_stable(array, size)) {
        ZF_LOGW("%s cycles are not stable\n", desc.name == NULL ? "unknown" : desc.name);
        if (ZF_LOG_LEVEL <= ZF_LOG_VERBOSE) {
            print_all(size, array);
        }
        if (!config_set(CONFIG_ALLOW_UNSTABLE_OVERHEAD)) {
            return (result_t) {0};
        }
    }

    for (int i = 0; i < size; i++) {
        array[i] -= desc.overhead;
    }

    return calculate_results(size, array);
}

void
process_results(size_t ncols, size_t nrows, ccnt_t array[ncols][nrows], result_desc_t desc,
                result_t results[ncols])
{
    for (int i = 0; i < ncols; i++) {
        results[i] = process_result(nrows, array[i], desc);
    }
}

