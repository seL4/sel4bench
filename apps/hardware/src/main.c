/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <hardware/gen_config.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>

#include <benchmark.h>
#include <hardware.h>

#define NOPS ""

#include <arch/hardware.h>

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void measure_nullsyscall_overhead(ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_NULLSYSCALL();
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

void measure_nullsyscall(ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_REAL_NULLSYSCALL();
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }

}

void measure_nullsyscall_ep(hardware_results_t *results)
{
    ccnt_t start, end, sum = 0, sum2 = 0, overhead;

    overhead = results->overhead_min;
    DATACOLLECT_INIT();

    for (seL4_Word i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_REAL_NULLSYSCALL();
        SEL4BENCH_READ_CCNT(end);
        DATACOLLECT_GET_SUMS(i, N_IGNORED, start, end, overhead, sum, sum2);
    }

    results->nullSyscall_ep_sum = sum;
    results->nullSyscall_ep_sum2 = sum2;
    results->nullSyscall_ep_num = N_RUNS - N_IGNORED;
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {0};

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(hardware_results_t),
              object_freq
          );
}

int main(int argc, char **argv)
{
    UNUSED int error;
    hardware_results_t *results;

    results = (hardware_results_t *) env->results;

    sel4bench_init();

    /* measure overhead */
    measure_nullsyscall_overhead(results->nullSyscall_overhead);
    measure_nullsyscall(results->nullSyscall_results);
    measure_nullsyscall_ep(results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
