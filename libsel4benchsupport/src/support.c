/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSDv2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <autoconf.h>

#include <sel4platsupport/plat/timer.h>
#include <sel4utils/process.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <benchmark.h>

#include <utils/util.h>

#include "support.h"

/* benchmarking environment */
static env_t env;

/* dummy global for libsel4muslcsys */
char _cpio_archive[1];

/* so malloc works */
#define MORE_CORE_SIZE 3000000
char *morecore_area;
size_t morecore_size;
static char app_morecore_area[MORE_CORE_SIZE];

/* allocator */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];
#define ALLOCMAN_VIRTUAL_SIZE BIT(20)

void
benchmark_putchar(int c)
{
    /* Benchmarks only print in debug mode */
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugPutChar(c);
#endif
}

NORETURN void
benchmark_finished(int exit_code)
{
    /* send back exit code */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_NoFault, 0, 0, 1);
    seL4_SetMR(0, exit_code);
    seL4_Send(SEL4UTILS_ENDPOINT_SLOT, info);
    /* we should not return */
    while (true);
}

static void
parse_code_region(sel4utils_elf_region_t *region)
{
    extern char __executable_start[];
    extern char _etext[];

    region->rights = seL4_AllRights;
    region->reservation_vstart = (void *) ROUND_DOWN((seL4_Word) __executable_start, (1 << seL4_PageBits));
    region->size = (void *) _etext - region->reservation_vstart;
    region->elf_vstart = region->reservation_vstart;
    region->cacheable = true;
}

static size_t
add_frames(void *frames[], size_t start, uintptr_t addr, size_t num_frames)
{
    size_t i;
    for (i = start; i < (num_frames + start); i++) {
        frames[i] = (void *) addr;
        addr += SIZE_BITS_TO_BYTES(seL4_PageBits);
    }

    return i;
}

static void
add_single_untyped(allocman_t *allocator, vka_t *vka, size_t untyped_size_bits,
                   uintptr_t *paddr, seL4_CPtr cap, int utType)
{
    cspacepath_t path;
    vka_cspace_make_path(vka, cap, &path);
    int error = allocman_utspace_add_uts(allocator, 1, &path, &untyped_size_bits, paddr,
                                     utType);
    if (error) {
        ZF_LOGF("Failed to add untyped to allocator");
    }
}

static allocman_t*
init_allocator(vka_t *vka, size_t untyped_size_bits,
               uintptr_t timer_paddr)
{
    /* set up malloc area */
    morecore_area = app_morecore_area;
    morecore_size = MORE_CORE_SIZE;

    /* set up allocator */
    allocman_t *allocator = bootstrap_use_current_1level(SEL4UTILS_CNODE_SLOT,
                                             CONFIG_SEL4UTILS_CSPACE_SIZE_BITS,
                                             FREE_SLOT,
                                             (1u << CONFIG_SEL4UTILS_CSPACE_SIZE_BITS),
                                             ALLOCATOR_STATIC_POOL_SIZE,
                                             allocator_mem_pool);
    /* create vka backed by allocator */
    allocman_make_vka(vka, allocator);

    /* add untyped memory */
    add_single_untyped(allocator, vka, untyped_size_bits, NULL, UNTYPED_SLOT,
                       ALLOCMAN_UT_KERNEL);
    add_single_untyped(allocator, vka, seL4_PageBits, &timer_paddr, TIMER_UNTYPED_SLOT,
                       ALLOCMAN_UT_DEV);
    return allocator;
}

static void
init_allocator_vspace(allocman_t *allocator, vspace_t *vspace)
{
    int error;
    /* Create virtual pool */
    reservation_t pool_reservation;
    void *vaddr;
    pool_reservation.res = allocman_mspace_alloc(allocator, sizeof(sel4utils_res_t), &error);
    if (!pool_reservation.res) {
        ZF_LOGF("Failed to allocate reservation");
    }
    error = sel4utils_reserve_range_no_alloc(vspace, pool_reservation.res,
                                             ALLOCMAN_VIRTUAL_SIZE, seL4_AllRights, 1, &vaddr);
    if (error) {
        ZF_LOGF("Failed to provide virtual memory allocator");
    }
    bootstrap_configure_virtual_pool(allocator, vaddr, ALLOCMAN_VIRTUAL_SIZE, SEL4UTILS_PD_SLOT);
}

static void
init_vspace(vka_t *vka, vspace_t *vspace, sel4utils_alloc_data_t *data,
            size_t stack_pages, uintptr_t stack_vaddr, uintptr_t results_addr,
            size_t results_bytes)
{
    int index;
    size_t results_size, ipc_buffer_size;

    /* set up existing frames - stack, ipc buffer, results */
    results_size = BYTES_TO_SIZE_BITS_PAGES(results_bytes, seL4_PageBits);
    ipc_buffer_size = BYTES_TO_SIZE_BITS_PAGES(sizeof(seL4_IPCBuffer), seL4_PageBits);
    void *existing_frames[stack_pages + results_size + ipc_buffer_size];


    index = add_frames(existing_frames, 0, results_addr, results_size);
    index = add_frames(existing_frames, index, (uintptr_t) seL4_GetIPCBuffer(), ipc_buffer_size);
    index = add_frames(existing_frames, index, stack_vaddr, stack_pages);

    if (sel4utils_bootstrap_vspace(vspace, data, SEL4UTILS_PD_SLOT, vka, NULL, NULL, existing_frames)) {
        ZF_LOGF("Failed to bootstrap vspace");
    }
}

static seL4_Error
get_irq(void *data, int irq, seL4_CNode cnode, seL4_Word index, uint8_t depth)
{
    assert(irq == DEFAULT_TIMER_INTERRUPT);
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, TIMEOUT_TIMER_IRQ_SLOT, seL4_WordBits);
    assert(error == seL4_NoError);

    return seL4_NoError;
}

static void
get_process_config(env_t *env, sel4utils_process_config_t *config, uint8_t prio, void *entry_point) 
{
    bzero(config, sizeof(sel4utils_process_config_t));
    config->is_elf = false;
    config->create_cspace = true;
    config->one_level_cspace_size_bits = CONFIG_SEL4UTILS_CSPACE_SIZE_BITS;
    config->create_vspace = true;
    config->reservations = &env->region;
    config->num_reservations = 1;
    config->create_fault_endpoint = false;
    config->fault_endpoint.cptr = 0; /* benchmark threads do not have fault eps */
    config->priority = prio;
    config->entry_point = entry_point;
    if (!config_set(CONFIG_X86_64)) {
        config->asid_pool = SEL4UTILS_ASID_POOL_SLOT;
    }
}

void
benchmark_shallow_clone_process(env_t *env, sel4utils_process_t *process, uint8_t prio, void *entry_point, 
                                char *name)
{
    int error;
    sel4utils_process_config_t config;

    get_process_config(env, &config, prio, entry_point);
    error = sel4utils_configure_process_custom(process, &env->vka, &env->vspace, config);
    
    if (error) {
        ZF_LOGF("Failed to configure process %s", name);
    }

    /* clone the text segment into the vspace - note that as we are only cloning the text
     * segment, you will not be able to use anything that relies on initialisation in benchmark
     * threads - like printf, (but seL4_Debug_PutChar is ok)
     */
    error = sel4utils_bootstrap_clone_into_vspace(&env->vspace, &process->vspace, 
                                                  env->region.reservation);
    if (error) {
        ZF_LOGF("Failed to bootstrap clone into vspace for %s", name);
    }

#ifdef CONFIG_DEBUG_BUILD
        seL4_DebugNameThread(process->thread.tcb.cptr, name);
#endif
}

void
benchmark_configure_thread_in_process(env_t *env, sel4utils_process_t *process,
                                      sel4utils_process_t *thread, uint8_t prio, void *entry_point, 
                                      char *name)
{
    int error;
    sel4utils_process_config_t config;

    get_process_config(env, &config, prio, entry_point);
    config.create_cspace = false;
    config.cnode = process->cspace;
    config.create_vspace = false;
    config.vspace = &process->vspace;

    error = sel4utils_configure_process_custom(thread, &env->vka, &env->vspace, config);
    if (error) {
        ZF_LOGF("Failed to configure process %s", name);
    }

#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(thread->thread.tcb.cptr, name);
#endif 
}

void
benchmark_configure_thread(env_t *env, seL4_CPtr fault_ep, uint8_t prio, char *name, sel4utils_thread_t *thread) 
{

    seL4_CapData_t guard = seL4_CapData_Guard_new(0, seL4_WordBits -
                                                     CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);

    int error = sel4utils_configure_thread(&env->vka, &env->vspace, &env->vspace, fault_ep, prio,
                                           SEL4UTILS_CNODE_SLOT, guard, thread);
    if (error) {
        ZF_LOGF("Failed to configure %s\n", name);
    }
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(thread->tcb.cptr, name);
#endif
}

void
benchmark_wait_children(seL4_CPtr ep, char *name, int num_children) 
{
    for (int i = 0; i < num_children; i++) {
        seL4_MessageInfo_t tag = seL4_Recv(ep, NULL);
        if (seL4_MessageInfo_get_label(tag) != seL4_NoFault) {
            /* failure - a thread we are waiting for faulted */
            sel4utils_print_fault_message(tag, name);
            abort();
        }
    }
}

env_t *
benchmark_get_env(int argc, char **argv, size_t results_size)
{
    size_t untyped_size_bits, stack_pages;
    uintptr_t stack_vaddr;

    /* parse arguments */
    if (argc < 5) {
        ZF_LOGF("Insufficient arguments, expected 5, got %d\n", (int) argc);
    }

    untyped_size_bits = (size_t) atol(argv[0]);
    stack_vaddr = (uintptr_t) atol(argv[1]);
    stack_pages = (size_t) atol(argv[2]);
    env.results = (void *) atol(argv[3]);
    env.timer_paddr = (uintptr_t) atol(argv[4]);

    env.allocman = init_allocator(&env.vka, untyped_size_bits, env.timer_paddr);
    init_vspace(&env.vka, &env.vspace, &env.data, stack_pages, stack_vaddr,
                (uintptr_t) env.results, results_size);
    init_allocator_vspace(env.allocman, &env.vspace);
    parse_code_region(&env.region);

    env.simple.arch_simple.irq = get_irq;
    benchmark_arch_get_simple(&env.simple.arch_simple);

    /* allocate a notification for timers */
    ZF_LOGF_IFERR(vka_alloc_notification(&env.vka, &env.ntfn), "Failed to allocate ntfn");
    /* get the timers */
    benchmark_arch_get_timers(&env);

    /* In case we used any FPU during our setup we will attempt to put the system
     * back into a steady state before returning */
#ifdef CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH
    for (int i = 0; i < CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH; i++) {
        seL4_Yield();
    }
#endif

    return &env;
}

