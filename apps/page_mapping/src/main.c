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
#include <vka/capops.h>
#include <sel4bench/arch/sel4bench.h>
#include <benchmark.h>
#include <page_mapping.h>

#include <sel4utils/mapping.h>

static inline int
alloc_pages(seL4_CPtr pages[], vka_t *vka, int npage, void* start_addr)
{
    int err = 0;
    void *addr = start_addr;

    for (int i = 0; i < npage; i++, addr += PAGE_SIZE_4K) {
        int nobj;
        vka_object_t tmp_obj[3]; // Max num of extra PT objects
        vka_object_t frame_obj;

        err = vka_alloc_frame(vka, PAGE_BITS_4K, &frame_obj);
        if (err) {
            ZF_LOGE_IFERR(err, "Failed to retype frame\n");
            return err;
        }

        pages[i] = frame_obj.cptr;

        /* Use sel4utils_map_page to map pages and allocate underlying 
         * page table structure */
        err = sel4utils_map_page(vka, SEL4UTILS_PD_SLOT, pages[i],
                                 addr, seL4_AllRights, true, tmp_obj, &nobj);
        if (err) {
            ZF_LOGE_IFERR(err, "Failed map pages\n");
            return err;
        }

        /* Unmap pages because benchmark assums pages unmapped to start with */
        err = seL4_ARCH_Page_Unmap(pages[i]);
        if (err) {
            ZF_LOGE_IFERR(err, "Failed unmap pages\n");
            return err;
        }
    }

    return err;
}

static void
run_bench(seL4_CPtr pages[], int npage, ccnt_t result[NPHASE], void* start_addr)
{
    ccnt_t start, end;
    int err UNUSED;
    void *addr = (void*)start_addr;

    /* Do the real mapping */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    for (int i = 0; i < npage; i++, addr += BIT(seL4_PageBits)) {
        err = seL4_ARCH_Page_Map(pages[i], SEL4UTILS_PD_SLOT, (seL4_Word)addr,
                                 seL4_AllRights, seL4_ARCH_Default_VMAttributes);
        assert(err == 0);
    }

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    result[MAP] = end - start;

    /* Protect mapped page as seL4_NoRights */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    for (int i = 0; i < npage; i++) {
        err = seL4_ARCH_Page_Remap(pages[i], SEL4UTILS_PD_SLOT, seL4_NoRights,
                                   seL4_ARCH_Default_VMAttributes);
        assert(err == 0);
    }

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    result[PROTECT] = end - start;


    /* Unprotect it back */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    for (int i = 0; i < npage; i++) {
        err = seL4_ARCH_Page_Remap(pages[i], SEL4UTILS_PD_SLOT, seL4_AllRights,
                                   seL4_ARCH_Default_VMAttributes);
        assert(err == 0);
    }

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    result[UNPROTECT] = end - start;

    /* Unmap pages*/
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    for (int i = 0; i < npage; i++) {
        err = seL4_ARCH_Page_Unmap(pages[i]);
        assert(err == 0);
    }

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    result[UNMAP] = end - start;
}

static void
measure_overhead(page_mapping_results_t *results)
{
    ccnt_t start, end;

    /* Measure cycle counter overhead  */
    for (int i = 0; i < RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        SEL4BENCH_READ_CCNT(start);

        SEL4BENCH_READ_CCNT(end);
        COMPILER_MEMORY_FENCE();
        results->overhead_benchmarks[i] = end - start;
    }
}

static int
start_bench(env_t *env, page_mapping_results_t *results, int max_page, 
            void* start_addr)
{
    seL4_CPtr pages[max_page];
    int err;

    err = alloc_pages(pages, &env->delegate_vka, max_page, start_addr);
    if (err) {
        return err;
    }

    for (int i = 0; i < RUNS; i++) {
        for (int j = 0; j < TESTS; j++) {
            /* run test */
            ccnt_t ret_time[NPHASE] = {0};
            run_bench(pages, page_mapping_benchmark_params[j].npage, ret_time,
                      start_addr);
            /* record result */
            for (int k = 0; k < NPHASE; k++) {
                results->benchmarks_result[j][k][i] = ret_time[k];
            }
        }
    }

    return err;
}

static inline int
get_max_page()
{
    int max_page = 0;

    for (int i = 0; i < TESTS; i++) {
        max_page = MAX(max_page, page_mapping_benchmark_params[i].npage);
    }

    return max_page;
}

int
main(int argc, char *argv[])
{
    env_t *env;
    page_mapping_results_t *results;
    void* start_addr;
    int max_page;

    static size_t object_freq[seL4_ObjectTypeCount] = {
#ifdef CONFIG_KERNEL_RT
        [seL4_SchedContextObject] = 1,
        [seL4_ReplyObject] = 1
#endif
    };

    env = benchmark_get_env(argc, argv, sizeof(page_mapping_results_t),
                            object_freq);

    /* Find maxium pages needed by benchmarks */
    max_page = get_max_page();

    /* Reserve a range of vspace enough to accomodate max_page 4K pages*/
    vspace_reserve_range(&env->vspace, max_page * PAGE_SIZE_4K, seL4_AllRights,
                 true, &start_addr);
    assert(IS_ALIGNED((seL4_Word)start_addr, seL4_PageBits));

    results = (page_mapping_results_t *)env->results;

    sel4bench_init();

    measure_overhead(results);

    if (start_bench(env, results, max_page, start_addr)) {
        benchmark_finished(EXIT_FAILURE);
    }
    else {
        benchmark_finished(EXIT_SUCCESS);
    }

    sel4bench_destroy();

    return 0;
}
