/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once

#include <stdbool.h>
#include <allocman/vka.h>
#include <sel4bench/sel4bench.h>
#include <sel4platsupport/timer.h>
#include <sel4rpc/client.h>
#include <platsupport/io.h>
#include <platsupport/irq.h>
#include <platsupport/ltimer.h>
#include <sel4utils/api.h>
#include <sel4utils/process.h>
#include <sel4utils/slab.h>
#include <simple/simple.h>
#include <vka/vka.h>
#include <vspace/vspace.h>
#include <benchmark_types.h>

/* average events = sel4bench generic events + the cycle counter */
#define NUM_AVERAGE_EVENTS (SEL4BENCH_NUM_GENERIC_EVENTS + 1u)
#define CYCLE_COUNT_EVENT SEL4BENCH_NUM_GENERIC_EVENTS
#define AVERAGE_RUNS 10000

#define MAX_TIMER_IRQS 4

/* benchmarking environment set up by root task */
typedef struct env {
    /* vka interface for allocating *fast* objects in the benchmark */
    vka_t slab_vka;
    /* vka interface for everything else */
    vka_t delegate_vka;
    /* vspace interface for managing virtual memory in the benchmark */
    vspace_t vspace;
    /* initialised allocman that backs the vka interface */
    allocman_t *allocman;
    /* minimal simple implementation that backs the default timer */
    simple_t simple;
    /* static data to initialise vspace */
    sel4utils_alloc_data_t data;
    /* code region so this app can shallow clone itself */
    sel4utils_elf_region_t region;
    /* virtual address to write benchmark results to */
    void *results;
    /* ltimer interface */
    ltimer_t ltimer;
    /* has the timer been initialised? */
    bool timer_initialised;
    /* notification that is bound to both timer irq handlers */
    vka_object_t ntfn;
    /* args we started with */
    benchmark_args_t *args;
    /* rpc client for communicating with benchmark driver */
    sel4rpc_client_t rpc_client;
    /* platsupport IO ops structure to use for the timer driver */
    ps_io_ops_t io_ops;
    /* ID of the notification managed by the mini IRQ interface */
    ntfn_id_t ntfn_id;
} env_t;

/* initialise the benchmarking environment and return it */
env_t *benchmark_get_env(int argc, char **argv, size_t results_size, size_t object_freq[seL4_ObjectTypeCount]);
/* signal to the benchmark driver process that we are done */
NORETURN void benchmark_finished(int exit_code);
/* Write for benchmarks. prints via a serial server in the initial task */
size_t benchmark_write(void *buf, size_t count);

/* Initialise the timer for the benchmark to use */
void benchmark_init_timer(env_t *env);

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
 * Spawn a thread or process within a shallow address space.
 *
 * This ensures that TLS is configured correctly within the address space
 * when it executes. This is needed to allow the thread to determine the
 * address of its IPC buffer.
 */
int benchmark_spawn_process(sel4utils_process_t *process, vka_t *vka, vspace_t *vspace, int argc,
                            char *argv[], int resume);

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
