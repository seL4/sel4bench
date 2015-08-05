/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Include Kconfig variables. */
#include <autoconf.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <assert.h>

#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
#include <simple-stable/simple-stable.h>
#else
#include <simple-default/simple-default.h>
#endif

#include <sel4platsupport/platsupport.h>
#include <sel4utils/stack.h>
#include <stdio.h>
#include <string.h>
#include <utils/util.h>
#include <vka/object.h>

#include "benchmark.h"

char _cpio_archive[1];

struct env global_env;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 100)

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* static memory for virtual memory bootstrapping */
static sel4utils_alloc_data_t data;

void
check_cpu_features(void)
{
#ifdef CONFIG_X86_64
    /* check if the cpu support rdtscp */
    int edx = 0;
    asm volatile("cpuid":"=d"(edx):"a"(0x80000001):"ecx");
    if ((edx & (1 << 27)) == 0) {
        printf("CPU does not support rdtscp instruction, halting\n");
        assert(0);
    }
#endif

}

static void
setup_fault_handler(env_t env)
{
    UNUSED int error;
    sel4utils_thread_t fault_handler;
    vka_object_t fault_ep = {0};

    printf("Setting up global fault handler...");

    /* create an endpoint */
    error = vka_alloc_endpoint(&env->vka, &fault_ep);
    assert(error == 0);

    /* set the fault endpoint for the current thread */
    error = seL4_TCB_SetSpace(simple_get_tcb(&env->simple), fault_ep.cptr,
                              simple_get_cnode(&env->simple), seL4_NilData, simple_get_pd(&env->simple),
                              seL4_NilData);
    assert(!error);

    error = sel4utils_start_fault_handler(fault_ep.cptr,
                                          &env->vka, &env->vspace, seL4_MaxPrio, simple_get_cnode(&env->simple),
                                          seL4_NilData,
                                          "sel4bench-fault-handler", &fault_handler);
    assert(!error);

}

void *main_continued(void *arg)
{
    UNUSED int error;
    printf("done\n");

    setup_fault_handler(&global_env);

    /* create benchmark endpoints */
    error = vka_alloc_endpoint(&global_env.vka, &global_env.ep);
    assert(error == 0);
    vka_cspace_make_path(&global_env.vka, global_env.ep.cptr, &global_env.ep_path);

    error = vka_alloc_endpoint(&global_env.vka, &global_env.result_ep);
    assert(error == 0);
    vka_cspace_make_path(&global_env.vka, global_env.result_ep.cptr, &global_env.result_ep_path);

    extern char __executable_start[];
    extern char _etext[];

    global_env.region.rights = seL4_AllRights;
    global_env.region.reservation_vstart = (void *) ROUND_DOWN((seL4_Word) __executable_start, (1 << seL4_PageBits));
    global_env.region.size = (void *) _etext - global_env.region.reservation_vstart;
    global_env.region.elf_vstart = global_env.region.reservation_vstart;
    global_env.region.cacheable = true;

    /* Run the benchmarking */
    printf("Doing benchmarks...\n\n");

    ipc_benchmarks_new(&global_env);

    printf("All is well in the universe.\n");
    printf("\n\nFin\n");

    return 0;
}

int main(void)
{

    seL4_BootInfo *info;
    allocman_t *allocman;
    UNUSED reservation_t virtual_reservation;
    UNUSED int error;
    void *vaddr;

    info  = seL4_GetBootInfo();

#ifdef CONFIG_KERNEL_STABLE
    simple_stable_init_bootinfo(&global_env.simple, seL4_GetBootInfo());
#else
    simple_default_init_bootinfo(&global_env.simple, seL4_GetBootInfo());
#endif

    /* create allocator */
    allocman = bootstrap_use_current_simple(&global_env.simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    assert(allocman);

    /* create vka */
    allocman_make_vka(&global_env.vka, allocman);

    /* create vspace */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&global_env.vspace, &data, simple_get_pd(&global_env.simple),
                                                           &global_env.vka, info);

    virtual_reservation = vspace_reserve_range(&global_env.vspace, ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights,
                                               1, &vaddr);
    assert(virtual_reservation.res);
    bootstrap_configure_virtual_pool(allocman, vaddr, ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&global_env.simple));

    /* init serial */
    platsupport_serial_setup_simple(NULL, &global_env.simple, &global_env.vka);

    /* Print welcome banner. */
    printf("\n");
    printf("%s%sse%sL4%s Benchmark%s\n", A_BOLD, A_FG_W, A_FG_G, A_FG_W, A_RESET);
    printf("%s==============%s\n", A_FG_G, A_FG_W);
    printf("\n");

    check_cpu_features();

    /* Switch to a bigger stack with some guard pages! */
    printf("Switching to a safer, bigger stack... ");
    error = (int) sel4utils_run_on_stack(&global_env.vspace, main_continued, NULL);
    assert(error == 0);

    return 0;
}
