/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include <simple/simple.h>
#include <sel4utils/process.h>
#include <vka/vka.h>
#include <vspace/vspace.h>

#ifdef CONFIG_ARCH_IA32
#define CCNT64BIT
#define CCNT_FORMAT "%llu"
typedef uint64_t ccnt_t;
#else
#define CCNT32BIT
typedef uint32_t ccnt_t;
#define CCNT_FORMAT "%d"
#endif

/* Contains information about the test environment. */
struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    simple_t simple;
    vspace_t vspace;

    /* ep to run benchmark on */
    vka_object_t ep;
    cspacepath_t ep_path;

    /* ep to send results on */
    vka_object_t result_ep;
    cspacepath_t result_ep_path;

    /* elf region containing code segment */
    sel4utils_elf_region_t region;

#ifdef CONFIG_KERNEL_STABLE
    seL4_CPtr asid_pool;
#endif /* CONFIG_KERNEL_STABLE */
};

typedef struct env *env_t;
typedef seL4_Word (*helper_func_t)(seL4_Word, seL4_Word, seL4_Word, seL4_Word);

void ipc_benchmarks_new(struct env* env);
void run_benchmarks(void);

#endif /* __BENCHMARK_H__ */
