/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <autoconf.h>
#include <sel4benchapp/gen_config.h>
#include <vcpu.h>
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <utils/util.h>

#include "../../benchmark.h"
#include "../../json.h"
#include "../../math.h"
#include "../../printing.h"
#include "../../processing.h"

#define VCPU_BENCHMARK_N_PARAMS     (0)

/** Processes the VCPU benchmark results.
 *
 * We don't parameterize the benchmarks or anything fancy, so nothing
 * particularly amazing is done in here.
 */
static json_t *process_vcpu_results(void *r)
{
    vcpu_benchmark_overall_results_t *raw_results = r;
    const int n_params = VCPU_BENCHMARK_N_PARAMS + 1;

    result_t results[VCPU_BENCHMARK_N_BENCHMARKS];

    result_set_t hvc_priv_esc_result_set = {
        .name = "HVC Privilege Escalation",
        .extra_cols = NULL,
        .n_extra_cols = 0,
        .results = &results[VCPU_BENCHMARK_HVC_PRIV_ESCALATE],
        .n_results = n_params,
    };

    result_set_t eret_priv_deesc_result_set = {
        .name = "ERET Privilege De-escalation",
        .extra_cols = NULL,
        .n_extra_cols = 0,
        .results = &results[VCPU_BENCHMARK_ERET_PRIV_DESCALATE],
        .n_results = n_params,
    };

    result_set_t hvc_null_kcall_result_set = {
        .name = "NULL HVC kernel hypercall invocation",
        .extra_cols = NULL,
        .n_extra_cols = 0,
        .results = &results[VCPU_BENCHMARK_HVC_NULL_SYSCALL],
        .n_results = n_params,
    };

    result_set_t sel4_call_ipc_result_set = {
        .name = "seL4_Call IPC from VCPU thread",
        .extra_cols = NULL,
        .n_extra_cols = 0,
        .results = &results[VCPU_BENCHMARK_CALL_SYSCALL],
        .n_results = n_params,
    };

    result_set_t sel4_reply_ipc_result_set = {
        .name = "seL4_Reply IPC to VCPU thread",
        .extra_cols = NULL,
        .n_extra_cols = 0,
        .results = &results[VCPU_BENCHMARK_REPLY_SYSCALL],
        .n_results = n_params,
    };

    result_set_t *bm_result_sets[VCPU_BENCHMARK_N_BENCHMARKS] = {
        &hvc_priv_esc_result_set,
        &eret_priv_deesc_result_set,
        &hvc_null_kcall_result_set,
        &sel4_call_ipc_result_set,
        &sel4_reply_ipc_result_set
    };

    result_desc_t all_bms_result_desc = {
        /* We already check for outlier results in the benchmark app. */
        .stable = false,
        .name = "VCPU benchmark",
        /* The overhead here is the overhead of the cycle counter register
         * read operation and not the overhead of the extra code paths which
         * are enabled in the kernel by BENCHMARK_TRACK_KERNEL_ENTRIES.
         */
        .overhead = raw_results->ccnt_read_overhead,
        .ignored = VCPU_BENCH_N_ITERATIONS_IGNORED
    };

    json_t *array = json_array();

    for (int i = 0; i < VCPU_BENCHMARK_N_BENCHMARKS; i++) {
        /* process_result basically takes a bunch of numbers and computes
         * averages, means, deviations, etc.
         */
        results[i] = process_result(VCPU_BENCH_N_ITERATIONS,
                                    raw_results->results[i].deep_data.elapsed,
                                    all_bms_result_desc);

        json_array_append_new(array, result_set_to_json(*bm_result_sets[i]));
    }

    return array;
}

#ifdef CONFIG_APP_VCPU_BENCH
static benchmark_t vcpu_benchmark = {
    .name = "vcpu",
    .enabled = config_set(CONFIG_APP_VCPU_BENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(
        sizeof(vcpu_benchmark_overall_results_t),
        seL4_PageBits),
    .process = process_vcpu_results,
    .init = blank_init
};

benchmark_t *vcpu_benchmark_new(void)
{
    return &vcpu_benchmark;
}
#endif
