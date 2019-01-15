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

#include <autoconf.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <string.h>
#include <stdarg.h>
#include <utils/util.h>
#include <utils/attribute.h>
#include <vka/vka.h>
#include <vka/object_capops.h>
#include <sel4/benchmark_track_types.h>

#include <benchmark.h>

#define VCPU_BENCH_COLLECT_DEEP_DATA            1
#define VCPU_BENCH_SHDATA_MAGIC                       (0xDEADCAB)
/* Assume 800 cycles is a sufficient anomalous deviance */
#define VCPU_BENCH_CYCLE_COUNTER_ANOMALY_THRESHOLD   (800)
#define VCPU_BENCH_NAME_SIZE                    (48)
#define VCPU_BENCH_N_GUESTS                     (1)
#define VCPU_BENCH_GUEST_STACK_SIZE             (BIT(seL4_PageBits) * 2)
#define VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE     VCPU_BENCH_GUEST_STACK_SIZE

#define VCPU_BENCH_N_ITERATIONS_IGNORED         (10)
#define VCPU_BENCH_N_ITERATIONS                 (100 + VCPU_BENCH_N_ITERATIONS_IGNORED)

// TX2 is Cortex A57 which has a 64B cache line size.
#define VCPU_BENCH_CACHE_LINE_SZ                (64)

#define VCPU_BENCH_MSG_IS_FAULT_BIT             (BIT(5))
#define VCPU_BENCH_MAX_N_GUESTS                 (VCPU_BENCH_MSG_IS_FAULT_BIT-1)

typedef struct vcpu_bm_shared_globals_ {
    /* This is given to the kernel for use as the log buffer for timestamps */
    vka_object_t log_buffer_frame_vkao;

#ifdef CONFIG_BENCHMARK_TRACK_KERNEL_ENTRIES
    /* This is our mapping of the log buffer so we can read the timestamps. */
    volatile benchmark_track_kernel_entry_t *sel4_log_buffer;
#endif
} vcpu_bm_shared_globals_t;

extern vcpu_bm_shared_globals_t sg;

enum hypcalls {
    /* Even though we namespace them, don't allow collision with fault type
     * label values.
     */
    HYPCALL_SYS_NOOP = 15,
    HYPCALL_SYS_PUTC,
    HYPCALL_SYS_PUTS,
    HYPCALL_SYS_GET_SEL4_CALL_END_STAMP,
    HYPCALL_SYS_GET_SEL4_REPLY_START_STAMP,
    HYPCALL_SYS_BM_REPORT_END_RESULTS,
    HYPCALL_SYS_BM_REPORT_DEEP_RESULTS,
    HYPCALL_SYS_EL1_FAULT,
    HYPCALL_SYS_EXIT_THREAD,
};

/* These threads will act as the guest VM threads with VCPUs bound to them. */
typedef struct _guest {
    int magic;
    int id, exitstatus;
    sel4utils_thread_t thread;
    vka_object_t vcpu_vkao;
    cspacepath_t badged_fault_ep, badged_fault_ep_ipc_copy;
    __attribute__((aligned(64))) uint8_t stack[VCPU_BENCH_GUEST_STACK_SIZE];
    char printf_buffer[VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE];
} guest_t;

extern guest_t guests[];
extern const char *vcpu_benchmark_names[];

enum vcpu_benchmarks {
    VCPU_BENCHMARK_HVC_PRIV_ESCALATE = 0,
    VCPU_BENCHMARK_ERET_PRIV_DESCALATE,
    VCPU_BENCHMARK_HVC_NULL_SYSCALL,
    VCPU_BENCHMARK_CALL_SYSCALL,
    VCPU_BENCHMARK_REPLY_SYSCALL,
    VCPU_BENCHMARK_N_BENCHMARKS,
};

typedef struct _vcpu_benchmark_deep_data {
    ccnt_t start[VCPU_BENCH_N_ITERATIONS],
             end[VCPU_BENCH_N_ITERATIONS],
             elapsed[VCPU_BENCH_N_ITERATIONS];
} vcpu_benchmark_deep_data_t;

typedef struct _vcpu_benchmark_results {
    char name[VCPU_BENCH_NAME_SIZE];
    /* These meta variables here are only used by this app internally for
     * debugging, so we can print and eyeball the numbers.
     * The process_vcpu_results() function computes averages using
     * process_result().
     */
    uint64_t min, max, clipped_avg, complete_avg, clipped_total, complete_total;
    int n_anomalies;
#if VCPU_BENCH_COLLECT_DEEP_DATA != 0
    vcpu_benchmark_deep_data_t deep_data;
#endif
} vcpu_benchmark_results_t;

typedef struct vcpu_benchmark_overall_results_ {
    ccnt_t ccnt_read_overhead;
    vcpu_benchmark_results_t results[VCPU_BENCHMARK_N_BENCHMARKS];
} vcpu_benchmark_overall_results_t;

NORETURN void
guest__start(void);
