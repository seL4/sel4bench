/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#ifndef __SELBENCH_MAPPING_H
#define __SELBENCH_MAPPING_H

#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>

#define RUNS 16
#define TESTS ARRAY_SIZE(page_mapping_benchmark_params)
#define NPHASE ARRAY_SIZE(phase_name)

typedef struct benchmark_params {
    /* name of the function we are benchmarking */
    const char* name;
    const int   npage;
} benchmark_params_t;

/* array of benchmarks to run */
static const
benchmark_params_t page_mapping_benchmark_params[] = {
        {
                .name   = "map 1",
                .npage  = 1,
        },
        {
                .name   = "map 2",
                .npage  = 2,
        },
        {
                .name   = "map 4",
                .npage  = 4,
        },
        {
                .name   = "map 8",
                .npage  = 8,
        },
        {
                .name   = "map 16",
                .npage  = 16,
        },
        {
                .name   = "map 32",
                .npage  = 32,
        },
        {
                .name   = "map 64",
                .npage  = 64,
        },
        {
                .name   = "map 128",
                .npage  = 128,
        },
        {
                .name   = "map 256",
                .npage  = 256,
        },
        {
                .name   = "map 512",
                .npage  = 512,
        },
        {
                .name   = "map 1024",
                .npage  = 1024,
        },
        {
                .name   = "map 2048",
                .npage  = 2048,
        },
};

char *phase_name[] = {
	"Prepare Page Tables",
	"Allocate Pages",
	"Mapping",
	"Protect Pages as Read Only",
	"Unprotect Pages"
};

typedef struct page_mapping_results {
    /* Raw results from benchmarking. These get checked for sanity */
    ccnt_t overhead_benchmarks[RUNS];
	ccnt_t benchmarks_result[TESTS][NPHASE][RUNS];
} page_mapping_results_t;

#endif /* __BENCH_MAPPING_H_H */
