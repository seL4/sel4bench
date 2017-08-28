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
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <autoconf.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/io.h>
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
    /* stop the timer */
    sel4platsupport_destroy_timer(&env.timer, &env.slab_vka);

    /* send back exit code */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
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

static allocman_t*
init_allocator(simple_t *simple, vka_t *vka, timer_objects_t *to)
{
    /* set up malloc area */
    morecore_area = app_morecore_area;
    morecore_size = MORE_CORE_SIZE;

    /* set up allocator */
    allocman_t *allocator = bootstrap_new_1level_simple(simple, CONFIG_SEL4UTILS_CSPACE_SIZE_BITS, ALLOCATOR_STATIC_POOL_SIZE,
                                                        allocator_mem_pool);

    ZF_LOGF_IF(!allocator, "Failed to initialize allocator");
    /* create vka backed by allocator */
    allocman_make_vka(vka, allocator);

    int error = allocman_add_untypeds_from_timer_objects(allocator, to);
    ZF_LOGF_IF(error, "Failed to add untypeds from timer to allocator");
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
    ZF_LOGF_IF(!pool_reservation.res, "Failed to allocate reservation");

    error = sel4utils_reserve_range_no_alloc(vspace, pool_reservation.res,
                                             ALLOCMAN_VIRTUAL_SIZE, seL4_AllRights, 1, &vaddr);
    ZF_LOGF_IF(error != 0, "Failed to provide virtual memory allocator");

    bootstrap_configure_virtual_pool(allocator, vaddr, ALLOCMAN_VIRTUAL_SIZE, SEL4UTILS_PD_SLOT);
}

static void
init_vspace(vka_t *vka, vspace_t *vspace, sel4utils_alloc_data_t *data,
            size_t stack_pages, uintptr_t stack_vaddr, uintptr_t results_addr,
            size_t results_bytes, uintptr_t args_vaddr)
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
    index = add_frames(existing_frames, index, (uintptr_t) args_vaddr, 1);

    if (sel4utils_bootstrap_vspace(vspace, data, SEL4UTILS_PD_SLOT, vka, NULL, NULL, existing_frames)) {
        ZF_LOGF("Failed to bootstrap vspace");
    }
}

static seL4_Error
get_irq(void *data, int irq, seL4_CNode cnode, seL4_Word index, uint8_t depth)
{
    env_t *env = data;
    seL4_CPtr cap = sel4platsupport_timer_objs_get_irq_cap(&env->args->to, irq, PS_INTERRUPT);
    cspacepath_t path;
    vka_cspace_make_path(&env->slab_vka, cap, &path);
    seL4_Error error = seL4_CNode_Move(cnode, index, depth,
                                       path.root, path.capPtr, path.capDepth);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to move irq cap");

    return seL4_NoError;
}

static inline sel4utils_process_config_t
get_process_config(env_t *env, uint8_t prio, void *entry_point)
{
    sel4utils_process_config_t config = process_config_new(&env->simple);
    config = process_config_noelf(config, entry_point, 0);
    config = process_config_create_cnode(config, CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    config = process_config_create_vspace(config, &env->region, 1);
    config = process_config_priority(config, prio);
#ifdef CONFIG_KERNEL_RT
    config.sched_params = sched_params_round_robin(config.sched_params, &env->simple, 0,
            CONFIG_BOOT_THREAD_TIME_SLICE * NS_IN_US);
#endif
    return process_config_mcp(config, prio);
}

void
benchmark_shallow_clone_process(env_t *env, sel4utils_process_t *process, uint8_t prio, void *entry_point,
                                char *name)
{
    int error;
    sel4utils_process_config_t config = get_process_config(env, prio, entry_point);
    error = sel4utils_configure_process_custom(process, &env->slab_vka, &env->vspace, config);
    ZF_LOGF_IFERR(error, "Failed to configure process %s", name);

    /* clone the text segment into the vspace - note that as we are only cloning the text
     * segment, you will not be able to use anything that relies on initialisation in benchmark
     * threads - like printf, (but seL4_Debug_PutChar is ok)
     */
    error = sel4utils_bootstrap_clone_into_vspace(&env->vspace, &process->vspace,
                                                  env->region.reservation);
    ZF_LOGF_IF(error, "Failed to bootstrap clone into vspace for %s", name);

    NAME_THREAD(process->thread.tcb.cptr, name);
}

void
benchmark_configure_thread_in_process(env_t *env, sel4utils_process_t *process,
                                      sel4utils_process_t *thread, uint8_t prio, void *entry_point,
                                      char *name)
{
    int error;
    sel4utils_process_config_t config = get_process_config(env, prio, entry_point);
    config = process_config_cnode(config, process->cspace);
    config = process_config_vspace(config, &process->vspace, process->pd);

    error = sel4utils_configure_process_custom(thread, &env->slab_vka, &env->vspace, config);
    ZF_LOGF_IFERR(error, "Failed to configure process %s", name);

    NAME_THREAD(thread->thread.tcb.cptr, name);
}

void
benchmark_configure_thread(env_t *env, seL4_CPtr fault_ep, uint8_t prio, char *name, sel4utils_thread_t *thread)
{
    sel4utils_thread_config_t config = thread_config_new(&env->simple);
    config = thread_config_fault_endpoint(config, fault_ep);
    config = thread_config_priority(config, prio);
    config = thread_config_mcp(config, prio);
#ifdef CONFIG_KERNEL_RT
    config.sched_params = sched_params_round_robin(config.sched_params, &env->simple, 0,
            CONFIG_BOOT_THREAD_TIME_SLICE * NS_IN_US);
#endif
    config = thread_config_create_reply(config);
    int error = sel4utils_configure_thread_config(&env->slab_vka, &env->vspace, &env->vspace, config, thread);
    ZF_LOGF_IF(error, "Failed to configure %s\n", name);
    NAME_THREAD(thread->tcb.cptr, name);
}

void
benchmark_wait_children(seL4_CPtr ep, char *name, int num_children)
{
    for (int i = 0; i < num_children; i++) {
        seL4_MessageInfo_t tag = api_wait(ep, NULL);
        if (seL4_MessageInfo_get_label(tag) != seL4_Fault_NullFault) {
            /* failure - a thread we are waiting for faulted */
            sel4utils_print_fault_message(tag, name);
            abort();
        }
    }
}

static int get_untyped_count(void *data)
{
    return 1;
}

static uint8_t get_cnode_size(void *data)
{
    return CONFIG_SEL4UTILS_CSPACE_SIZE_BITS;
}

static seL4_CPtr get_nth_untyped(void *data, int n, size_t *size_bits, uintptr_t *paddr, bool *device)
{
    env_t *env = data;
    if (n != 0) {
        ZF_LOGE("Asked for untyped we don't have");
        return seL4_CapNull;
    }

    if (size_bits) {
        *size_bits = env->args->untyped_size_bits;
    }

    if (device) {
        *device = false;
    }

    if (paddr) {
        *paddr = 0;
    }
    return env->args->untyped_cptr;
}

static int get_cap_count(void *data) {
    /* Our parent only tells us our first free slot, so we have to work out from that how many
       capabilities we actually have. We assume we have cspace layout from sel4utils_cspace_layout
       and so we have 1 empty null slot, and if we're not on the RT kernel two more unused slots */
    int last = ((env_t *) data)->args->first_free;

    /* skip the null slot */
    last--;

    if (config_set(CONFIG_X86_64)) {
        /* skip the ASID pool slot */
        last--;
    }

    if (!config_set(CONFIG_KERNEL_RT)) {
        /* skip the 2 RT only slots */
        last -= 2;
    }

    return last;
}

static seL4_CPtr get_nth_cap(void *data, int n)
{
    if (n < get_cap_count(data)) {
        /* need to turn the contiguous index n into a potentially non contiguous set of
           cptrs, in accordance with the holes discussed in get_cap_count. */
        /* first convert our index that starts at 0, to one starting at 1, this removes the
         the hole of the null slot */
        n++;

        /* introduce a 1 cptr hole if we on x86_64, as it does not have an ASID pool */
        if (config_set(CONFIG_X86_64) && n >= SEL4UTILS_ASID_POOL_SLOT) {
            n++;
        }
        /* now introduce a 2 cptr hole if we're not on the RT kernel */
        if (!config_set(CONFIG_KERNEL_RT) && n >= SEL4UTILS_SCHED_CONTEXT_SLOT) {
            n += 2;
        }
        return n;
    }
    return seL4_CapNull;
}

static int get_core_count(void *data)
{
    return ((env_t *) data)->args->nr_cores;
}

static seL4_CPtr sched_ctrl(void *data, int core)
{
    return ((env_t *) data)->args->sched_ctrl + core;
}

static void init_simple(env_t *env)
{
    env->simple.data = env;

    //env->simple.frame_cap =
    //env->simple.frame_mapping =
    //env->simple.frame_info =
    //env->simple.ASID_assign =

    env->simple.cap_count = get_cap_count;
    env->simple.nth_cap = get_nth_cap;
    env->simple.init_cap = sel4utils_process_init_cap;
    env->simple.cnode_size = get_cnode_size;
    env->simple.untyped_count = get_untyped_count;
    env->simple.nth_untyped = get_nth_untyped;
    env->simple.core_count = get_core_count;
    env->simple.sched_ctrl = sched_ctrl;
    //env->simple.userimage_count =
    //env->simple.nth_userimage =
    //env->simple.print =
    //env->simple.arch_info =
    //env->simple.extended_bootinfo =

    env->simple.arch_simple.data = env;
    env->simple.arch_simple.irq = get_irq;
    benchmark_arch_get_simple(&env->simple.arch_simple);
}

env_t *
benchmark_get_env(int argc, char **argv, size_t results_size, size_t object_freq[seL4_ObjectTypeCount])
{
    /* parse arguments */
    if (argc < 1) {
        ZF_LOGF("Insufficient arguments, expected 1, got %d\n", (int) argc);
    }

    env.args = (void *) atol(argv[0]);
    env.results = env.args->results;
    init_simple(&env);

    env.allocman = init_allocator(&env.simple, &env.delegate_vka, &env.args->to);
    init_vspace(&env.delegate_vka, &env.vspace, &env.data, env.args->stack_pages, env.args->stack_vaddr,
                (uintptr_t) env.results, results_size, (uintptr_t) env.args);
    init_allocator_vspace(env.allocman, &env.vspace);
    parse_code_region(&env.region);

    /* In case we used any FPU during our setup we will attempt to put the system
     * back into a steady state before returning */
#ifdef CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH
    for (int i = 0; i < CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH; i++) {
        seL4_Yield();
    }
#endif

    /* update object freq for timer objects */
    object_freq[seL4_NotificationObject]++;
    object_freq[seL4_ARCH_4KPage] += env.args->to.nobjs;

    /* set up slab allocator */
    if (slab_init(&env.slab_vka, &env.delegate_vka, object_freq) != 0) {
        ZF_LOGF("Failed to init slab allocator");
    }

    /* allocate a notification for timers */
    ZF_LOGF_IFERR(vka_alloc_notification(&env.slab_vka, &env.ntfn), "Failed to allocate ntfn");

    /* set up irq caps */
    int error = sel4platsupport_init_timer_irqs(&env.slab_vka, &env.simple, env.ntfn.cptr, &env.timer, &env.args->to);
    ZF_LOGF_IF(error, "Failed to init timer irqs");

    ps_io_ops_t ops;
    error = sel4platsupport_new_io_mapper(env.vspace, env.slab_vka, &ops.io_mapper);
    ZF_LOGF_IF(error, "Failed to init io mapper");

    error = sel4platsupport_new_malloc_ops(&ops.malloc_ops);
    ZF_LOGF_IF(error, "Failed to init io mapper");

    benchmark_arch_get_timers(&env, ops);
    return &env;
}

void
send_result(seL4_CPtr ep, ccnt_t result)
{
    int length = sizeof(ccnt_t) / sizeof(seL4_Word);
    unsigned int shift = length > 1u ? seL4_WordBits : 0;
    for (int i = length - 1; i >= 0; i--) {
        seL4_SetMR(i, (seL4_Word) result);
        result = result >> shift;
    }

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, (seL4_Uint32)length);
    seL4_Send(ep, tag);
}

ccnt_t
get_result(seL4_CPtr ep)
{
    ccnt_t result = 0;
    seL4_MessageInfo_t tag = api_wait(ep, NULL);
    int length = seL4_MessageInfo_get_length(tag);
    unsigned int shift = length > 1u ? seL4_WordBits : 0;

    for (int i = 0; i < length; i++) {
        result = result << shift;
        result += seL4_GetMR(i);
    }

    return result;
}
