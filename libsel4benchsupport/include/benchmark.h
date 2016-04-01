/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __BENCHMARK_H
#define __BENCHMARK_H

#include <allocman/vka.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <simple/simple.h>
#include <vka/vka.h>
#include <vspace/vspace.h>

#include <cspace.h>

/* benchmarking environment set up by root task */
typedef struct env {
    /* vka interface for allocating cslots and objects in the benchmark */
    vka_t vka;
    /* vspace interface for managing virtual memory in the benchmark */
    vspace_t vspace;
    /* intialised allocman that backs the vka interface */
    allocman_t allocman;
    /* minimal simple implementation that backs the default timer */
    simple_t simple;
    /* static data to initialise vspace */
    sel4utils_alloc_data_t data;
    /* code region so this app can 'fork' itself */
    sel4utils_elf_region_t region;
    /* virtual address to write benchmark results to */
    void *results;
} env_t;

/* initialise the benchmarking environment and return it */
env_t *benchmark_get_env(int argc, char **argv, size_t results_size);
NORETURN void benchmark_finished(int exit_code);
void benchmark_putchar(int c);

void benchmark_shallow_clone_process(env_t *env, sel4utils_process_t *process, uint8_t prio, 
                                    void *entry_point, char *name);
void benchmark_configure_thread_in_process(env_t *env, sel4utils_process_t *process, 
                                          sel4utils_process_t *thread, uint8_t prio, 
                                          void *entry_point, char *name); 

void
benchmark_configure_thread(env_t *env, seL4_CPtr fault_ep, uint8_t prio, char *name, 
                           sel4utils_thread_t *thread);
#endif /* __BENCHMARK_H__ */
