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
#include <autoconf.h>
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <utils/util.h>
#include <page_mapping.h>

#include "benchmark.h"
#include "json.h"
#include "math.h"
#include "printing.h"
#include "processing.h"

static json_t *
process_mapping_results(void *r)
{
    page_mapping_results_t *raw_results = r;
    ccnt_t overhead;

    /* check overheads */
    result_desc_t desc = {
            .stable = false,
            .name = "overhead",
            .ignored = 0,
            .overhead = 0
    };
    result_t overhead_result = process_result(RUNS,
                                              raw_results->overhead_benchmarks,
                                              desc);

    overhead = overhead_result.min;

    int nline = TESTS * NPHASE;

	char *phase_col[nline];
	json_int_t npage_col[nline];
	for (int i = 0; i < nline; i++) {
		phase_col[i] = phase_name[i % NPHASE];
		npage_col[i] = page_mapping_benchmark_params[i/NPHASE].npage;
	}

    column_t extra_cols[] = {
            {
                    .header = "Num of Page Mapped",
                    .type = JSON_INTEGER,
                    .integer_array = npage_col,
            },
			{
					.header = "Phase",
					.type = JSON_STRING,
					.string_array = phase_col,
			},
    };

    result_t results[TESTS][NPHASE];

    result_set_t result_set = {
            .name = "Mapping Benchmark",
            .extra_cols = extra_cols,
            .n_extra_cols = ARRAY_SIZE(extra_cols),
            .results = (result_t *)results,
            .n_results = nline,
    };

    /* now calculate the results */
    for (int i = 0; i < TESTS; i++) {
		for (int j = 0; j < NPHASE; j++){
	        result_desc_t desc = {
					.name = page_mapping_benchmark_params[i].name,
					.overhead = overhead,
	        };
			results[i][j] =
				process_result(RUNS,raw_results->benchmarks_result[i][j],desc);
		}
    }

    json_t *array = json_array();
    json_array_append_new(array, result_set_to_json(result_set));
    return array;
}

static benchmark_t page_mapping_benchmark = {
        .name = "page_mapping",
        .enabled = config_set(CONFIG_APP_PAGEMAPPINGBENCH),
        .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(page_mapping_results_t),
                                                  seL4_PageBits),
        .process = process_mapping_results,
        .init = blank_init
};

benchmark_t *
page_mapping_benchmark_new(void)
{
    return &page_mapping_benchmark;
}
