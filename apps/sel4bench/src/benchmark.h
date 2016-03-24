/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <simple/simple.h>
#include <vka/vka.h>

typedef struct benchmark {
    /* name of the benchmark application */
    char *name;
    /* size of data structure required to store results */
    size_t results_pages;
    /*
     * Process and output the results
     * 
     */
    void (*process)(void *results);
    /* carry out any extra init for this process */
    void (*init)(vka_t *vka, simple_t *simple, sel4utils_process_t *process);
} benchmark_t;

/* generic result type */
typedef struct result {
    double variance;
    double stddev;
    double stddev_pc;
    double mean;
    ccnt_t min;
    ccnt_t max;
} result_t;

benchmark_t ipc_benchmark_new(void);
benchmark_t irq_benchmark_new(void);

#endif /* BENCHMARK_H */
