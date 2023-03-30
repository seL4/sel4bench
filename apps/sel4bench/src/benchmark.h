/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <sel4benchapp/gen_config.h>
#include <sel4benchfault/gen_config.h>
#include <hardware/gen_config.h>
#include <sel4benchipc/gen_config.h>
#include <sel4benchirq/gen_config.h>
#include <sel4benchirquser/gen_config.h>
#include <sel4benchpagemapping/gen_config.h>
#include <sel4benchscheduler/gen_config.h>
#include <sel4benchsignal/gen_config.h>
#include <smp/gen_config.h>
#include <sel4benchsync/gen_config.h>
#include <sel4benchvcpu/gen_config.h>
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <simple/simple.h>
#include <vka/vka.h>

typedef struct benchmark {
    /* name of the benchmark application */
    char *name;
    /* should we run this benchmark */
    bool enabled;
    /* size of data structure required to store results */
    size_t results_pages;
    /*
     * Process and return the results
     *
     */
    json_t *(*process)(void *results);
    /* carry out any extra init for this process */
    void (*init)(vka_t *vka, simple_t *simple, sel4utils_process_t *process);
} benchmark_t;

/* generic result type */
typedef struct {
    double variance;
    double stddev;
    double mean;
    ccnt_t min;
    ccnt_t max;
    ccnt_t mode;
    double median;
    double first_quantile;
    double third_quantile;
    size_t samples;
    ccnt_t *raw_data;
} result_t;

typedef struct {
    /* header to print at top of column */
    char *header;
    /* pointer to first element in array of column values - length must match n_results in the
     * result_set_t that this column is used with. */
    union {
        char **string_array;
        json_int_t *integer_array;
        double *real_array;
        bool *bool_array;
    };
    /* type of the column */
    json_type type;
} column_t;

/* describes result output */
typedef struct {
    /* name of the result set */
    const char *name;
    /* columns to prepend to result output */
    column_t *extra_cols;
    /* number of extra columns */
    int n_extra_cols;
    /* array of results */
    result_t *results;
    /* number of results in this set */
    int n_results;
} result_set_t;

/* description of how to process a result */
typedef struct {
    /* error if result should be stable */
    bool stable;
    /* name of result */
    const char *name;
    /* overhead to subtract from each result before calculations */
    ccnt_t overhead;
    /* number of samples to ignore (for cold cache values). */
    int ignored;
} result_desc_t;

benchmark_t *ipc_benchmark_new(void);
benchmark_t *irq_benchmark_new(void);
benchmark_t *irquser_benchmark_new(void);
benchmark_t *scheduler_benchmark_new(void);
benchmark_t *signal_benchmark_new(void);
benchmark_t *fault_benchmark_new(void);
benchmark_t *hardware_benchmark_new(void);
benchmark_t *sync_benchmark_new(void);
benchmark_t *page_mapping_benchmark_new(void);
benchmark_t *smp_benchmark_new(void);
benchmark_t *vcpu_benchmark_new(void);
/* Add new benchmarks here */

static inline void blank_init(UNUSED vka_t *vka, UNUSED simple_t *simple, UNUSED sel4utils_process_t *process)
{
    /* for benchmarks with no specific init */
}
