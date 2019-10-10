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
#include <autoconf.h>
#include <sel4benchpagemapping/gen_config.h>
#include <vka/capops.h>
#include <sel4bench/arch/sel4bench.h>
#include <benchmark.h>
#include <page_mapping.h>

#define START_ADDR 0x60000000
#define NUM_ARGS 3

#if defined(CONFIG_ARCH_X86_64)
#define DEFAULT_DEPTH 64
#else
#define DEFAULT_DEPTH 32
#endif /* defined(CONFIG_ARCH_X86_64) */

#define PAGE_PER_TABLE BIT(seL4_PageTableBits - seL4_WordSizeBits)
#define untyped_retype_root(a,b,c,e)\
        seL4_Untyped_Retype(a,b,c,SEL4UTILS_CNODE_SLOT,\
                        SEL4UTILS_CNODE_SLOT,DEFAULT_DEPTH,e,1)
#define NUM_PAGE_TABLE(npage) DIV_ROUND_UP((npage), PAGE_PER_TABLE)

typedef struct helper_thread {
    sel4utils_process_t process;
    seL4_CPtr untyped;
    seL4_CPtr result_ep;
    int npage;
    char *argv[NUM_ARGS];
    char argv_strings[NUM_ARGS][WORD_STRING_SIZE];
} helper_thread_t;

static void inline prepare_page_table(seL4_Word addr, int npage, seL4_CPtr untyped,
                                      seL4_CPtr *free_slot)
{
    long err UNUSED;
    for (int i = 0; i < NUM_PAGE_TABLE(npage); i++) {
        seL4_CPtr pt = *free_slot;
        err = untyped_retype_root(untyped, seL4_ARCH_PageTableObject,
                                  seL4_PageTableBits, pt);
        assert(err == 0);
        (*free_slot)++;

        err = seL4_ARCH_PageTable_Map(pt, SEL4UTILS_PD_SLOT, addr,
                                      seL4_ARCH_Default_VMAttributes);

#if defined(CONFIG_ARCH_X86_64)
        if (err) {
            seL4_CPtr pd = *free_slot;
            err = untyped_retype_root(untyped, seL4_X86_PageDirectoryObject,
                                      seL4_PageDirBits, pd);
            assert(err == 0);
            (*free_slot)++;
            err = seL4_X86_PageDirectory_Map(pd, SEL4UTILS_PD_SLOT, addr,
                                             seL4_ARCH_Default_VMAttributes);

            if (err) {
                seL4_CPtr pdpt = *free_slot;
                err = untyped_retype_root(untyped, seL4_X86_PDPTObject,
                                          seL4_PDPTBits, pdpt);
                assert(err == 0);
                (*free_slot)++;
                err = seL4_X86_PDPT_Map(pdpt, SEL4UTILS_PD_SLOT, addr,
                                        seL4_ARCH_Default_VMAttributes);
                err = seL4_X86_PageDirectory_Map(pd, SEL4UTILS_PD_SLOT, addr,
                                                 seL4_ARCH_Default_VMAttributes);
            }

            err = seL4_ARCH_PageTable_Map(pt, SEL4UTILS_PD_SLOT, addr,
                                          seL4_ARCH_Default_VMAttributes);
        }

#endif /* CONFIG_ARCH_X86_64 */
        assert(err == 0);
        addr += PAGE_SIZE_4K * PAGE_PER_TABLE;
    }
}

static void inline prepare_pages(int npage, seL4_CPtr untyped,
                                 seL4_CPtr *free_slot)
{
    long err UNUSED;
    for (int i = 0; i < npage; i++) {
        err = untyped_retype_root(untyped, seL4_ARCH_4KPage, seL4_PageBits,
                                  *free_slot);
        assert(err == 0);

        (*free_slot)++;
    }
}

static void inline map_pages(seL4_CPtr addr, seL4_CPtr page_cap, int npage)
{
    long err UNUSED;
    for (int i = 0; i < npage; i++) {
        err = seL4_ARCH_Page_Map(page_cap, SEL4UTILS_PD_SLOT, addr,
                                 seL4_AllRights, seL4_ARCH_Default_VMAttributes);
        assert(err == 0);
        addr += PAGE_SIZE_4K;
        page_cap++;
    }
}

#define PROT_UNPROT_NPAGE(func, right)\
    static void inline func(seL4_CPtr addr, seL4_CPtr page_cap, int npage){\
        long err UNUSED;\
        for(int i = 0; i < npage; i++){\
            err = seL4_ARCH_Page_Map(page_cap, SEL4UTILS_PD_SLOT, addr, right,\
                            seL4_ARCH_Default_VMAttributes);\
            assert(err == 0);\
            page_cap++;\
        }\
    }
PROT_UNPROT_NPAGE(prot_pages, seL4_CanRead)
PROT_UNPROT_NPAGE(unprot_pages, seL4_AllRights)

/* Benchmark Child Process */
static void
bench_proc(int argc UNUSED, char *argv[])
{
    seL4_CPtr result_ep = (seL4_CPtr)atoi(argv[0]);
    seL4_CPtr untyped = (seL4_CPtr)atoi(argv[1]);
    int npage = atoi(argv[2]);
    seL4_CPtr free_slot = untyped + 1;
    seL4_Word addr = START_ADDR;
    ccnt_t start, end;

    sel4bench_init();

    seL4_CPtr pt_ptr_start = free_slot;
    /* Install page tables to avoid mapping to fail */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    prepare_page_table(addr, npage, untyped, &free_slot);

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, end - start);

    seL4_CPtr page_ptr_start = free_slot;
    /* allocate pages */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    prepare_pages(npage, untyped, &free_slot);

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, end - start);

    /* Do the real mapping */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    map_pages(addr, page_ptr_start, npage);

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, end - start);

    /* Protect mapped page as seL4_CanRead */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    prot_pages(addr, page_ptr_start, npage);

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, end - start);

    /* Unprotect it back */
    COMPILER_MEMORY_FENCE();
    SEL4BENCH_READ_CCNT(start);

    unprot_pages(addr, page_ptr_start, npage);

    SEL4BENCH_READ_CCNT(end);
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, end - start);

    /* cleaning */
    long err;
    for (int i = 0; i < npage; i++) {
        err = seL4_ARCH_Page_Unmap(page_ptr_start + i);
        ZF_LOGF_IFERR(err, "ummap page failed\n");
    }

    for (int i = 0; i < NUM_PAGE_TABLE(npage); i++) {
        err = seL4_ARCH_PageTable_Unmap(pt_ptr_start + i);
        ZF_LOGF_IFERR(err, "ummap page table failed\n");

    }

    sel4bench_destroy();
}

static void run_bench_child_proc(env_t *env,
                                 cspacepath_t *result_ep_path,
                                 ccnt_t result[NPHASE],
                                 helper_thread_t *proc
                                )
{

    if (benchmark_spawn_process(&proc->process, &env->delegate_vka, &env->vspace,
                                NUM_ARGS, proc->argv, 1) != 0) {
        ZF_LOGF("Failed to spawn client\n");
    }

    /* Get result from benchmarking process */
    for (int i = 0; i < NPHASE; i++) {
        result[i] = get_result(result_ep_path->capPtr);
    }
}

static void measure_overhead(page_mapping_results_t *results)
{
    ccnt_t start, end;

    sel4bench_init();

    /* Measure cycle counter overhead  */
    for (int i = 0; i < RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        SEL4BENCH_READ_CCNT(start);

        SEL4BENCH_READ_CCNT(end);
        COMPILER_MEMORY_FENCE();
        results->overhead_benchmarks[i] = end - start;
    }

    sel4bench_destroy();
}

int main(int argc, char *argv[])
{
    env_t *env;
    page_mapping_results_t *results;
    vka_object_t result_ep, untyped_obj;
    cspacepath_t result_ep_path, untyped_path;
    helper_thread_t proc;

    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 1,
        [seL4_EndpointObject] = 1,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 1,
        [seL4_ReplyObject] = 1
#endif
    };

    env = benchmark_get_env(argc, argv, sizeof(page_mapping_results_t),
                            object_freq);
    results = (page_mapping_results_t *)env->results;

    /* allocate result end point */
    if (vka_alloc_endpoint(&env->slab_vka, &result_ep) != 0) {
        ZF_LOGF("Failed to allocate endpoint");
    }
    vka_cspace_make_path(&env->slab_vka, result_ep.cptr, &result_ep_path);

    /* allocate untyped cap for pages */
    if ((vka_alloc_untyped(&env->delegate_vka, env->args->untyped_size_bits - 2,
                           &untyped_obj)) != 0) {
        ZF_LOGF("alloc untyped fail\n");
    };
    vka_cspace_make_path(&env->delegate_vka, untyped_obj.cptr, &untyped_path);

    /* Create a new process to run test
     * shallow clone only copy .text segment
     * */
    benchmark_shallow_clone_process(env, &proc.process, seL4_MaxPrio,
                                    bench_proc, "Proc");

    proc.result_ep = sel4utils_copy_path_to_process(&proc.process,
                                                    result_ep_path);

    measure_overhead(results);
    for (int i = 0; i < RUNS; i++) {
        for (int j = 0; j < TESTS; j++) {
            proc.untyped = sel4utils_copy_path_to_process(&proc.process,
                                                          untyped_path);
            proc.npage = page_mapping_benchmark_params[j].npage;

            sel4utils_create_word_args(proc.argv_strings, proc.argv, NUM_ARGS,
                                       proc.result_ep, proc.untyped, proc.npage);

            /* run test */
            ccnt_t ret_time[NPHASE] = {0};
            run_bench_child_proc(env, &result_ep_path, ret_time, &proc);
            /* record result */
            for (int k = 0; k < NPHASE; k++) {
                results->benchmarks_result[j][k][i] = ret_time[k];
            }
            vka_cnode_revoke(&untyped_path);

            /* Manually set next free slot to make sure untyped cap is set
             * at the same slot every time*/
            proc.process.cspace_next_free--;

            /* suspend proc to be reused */
            seL4_TCB_Suspend(proc.process.thread.tcb.cptr);
        }
    }
    vka_free_object(&env->delegate_vka, &untyped_obj);

    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
