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
#ifndef __BENCHMARK_H
#define __BENCHMARK_H

#include <allocman/vka.h>
#include <sel4bench/sel4bench.h>
#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4utils/process.h>
#include <sel4utils/slab.h>
#include <simple/simple.h>
#include <vka/vka.h>
#include <vspace/vspace.h>

#include <cspace.h>

/* average events = sel4bench generic events + the cycle counter */
#define NUM_AVERAGE_EVENTS (SEL4BENCH_NUM_GENERIC_EVENTS + 1u)
#define CYCLE_COUNT_EVENT SEL4BENCH_NUM_GENERIC_EVENTS
#define AVERAGE_RUNS 10000

/* benchmarking environment set up by root task */
typedef struct env {
    /* vka interface for allocating *fast* objects in the benchmark */
    vka_t slab_vka;
    /* vka interface for everything else */
    vka_t delegate_vka;
    /* vspace interface for managing virtual memory in the benchmark */
    vspace_t vspace;
    /* intialised allocman that backs the vka interface */
    allocman_t *allocman;
    /* minimal simple implementation that backs the default timer */
    simple_t simple;
    /* static data to initialise vspace */
    sel4utils_alloc_data_t data;
    /* code region so this app can shallow clone itself */
    sel4utils_elf_region_t region;
    /* virtual address to write benchmark results to */
    void *results;
    /* timeout timer that can be used to set timeouts */
    seL4_timer_t *timeout_timer;
    /* notification that is bound to both timer irq handlers */
    vka_object_t ntfn;
    /* paddr for timeout timer */
    uintptr_t timer_paddr;
    /* size of untyped passed to benchmarks */
    size_t untyped_size_bits;
    /* number of core available for benchmark use */
    int nr_cores;
} env_t;

/* initialise the benchmarking environment and return it */
env_t *benchmark_get_env(int argc, char **argv, size_t results_size, size_t object_freq[seL4_ObjectTypeCount]);
/* signal to the benchmark driver process that we are done */
NORETURN void benchmark_finished(int exit_code);
/* put char for benchmarks, does not print if kernel is not a debug kernel */
void benchmark_putchar(int c);

/* Child thread/process helper functions */

/* Create a new process that is a shallow clone of the current process.
 *
 * This is *not* fork, and will only copy the text segment
 * into the new address space. This results in very light weight processes.
 *
 * This means you can start your new process at any function in the text segment,
 * but those functions cannot rely on any globally initialised data in the heap or the stack
 * Most significantly, any c library calls that rely on the vsyscall table will not work
 * (like abort, or printf).
 *
 * @param env environment from benchmark_get_env
 * @param[out] process the process to create
 * @param prio the priority to run the process at
 * @param entry_point a function in the text segment to start the process at
 * @param name a name for the thread for debugging (this will be passed to seL4_DebugNameThread)
 * @param fault_ep fault endpoint for the thread
 */
void benchmark_shallow_clone_process(env_t *env, sel4utils_process_t *process, uint8_t prio,
                                    void *entry_point, char *name);

/*
 * Create a new thread in a shallow processes address space.
 *
 * @param env environment from benchmark_get_env
 * @param process to create the thread in the address space of
 * @param[out] thread the thread to create
 * @param prio the priority to run the process at
 * @param entry_point a function in the text segment to start the process at
 * @param name a name for the thread for debugging (this will be passed to seL4_DebugNameThread)
 */
void benchmark_configure_thread_in_process(env_t *env, sel4utils_process_t *process,
                                          sel4utils_process_t *thread, uint8_t prio,
                                          void *entry_point, char *name);

/*
 * Configure a thread in the address space of the current environment.
 *
 * @param env environment from benchmark_get_env
 * @param fault_ep fault endpoint for the thread
 * @param prio the priority to run the process at
 * @param name a name for the thread for debugging (this will be passed to seL4_DebugNameThread)
 * @param[out] thread to create
 */
void benchmark_configure_thread(env_t *env, seL4_CPtr fault_ep, uint8_t prio, char *name,
                                sel4utils_thread_t *thread);

/*
 * Wait for n child threads/processes to terminate successfully.
 *
 * The child thread/processes must signal on ep when finished, *and* have ep also
 * set as their fault endpoint. This way if a child terminates we will get an IPC.
 *
 * If any child terminates with a fault, abort. Otherwise, return when all children are
 * finished.
 *
 * @param ep the endpoint children will signal or fault on when done
 * @param name a name for the child for debugging output on fault.
 * @param num_children the number of children to wait for
 */
void benchmark_wait_children(seL4_CPtr ep, char *name, int num_children);

/*
 * Send the cycle counter result through given endpoint
 *
 * @param ep The endpoint the result will be send through
 * @param result The conteng to be sent
 */
void send_result(seL4_CPtr ep, ccnt_t result);

/*
 * Receive the cycle counter result from given endpoint
 *
 * @param ep The endpoint the result will be received from
 */
ccnt_t get_result(seL4_CPtr ep);

#endif /* __BENCHMARK_H__ */
