/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <autoconf.h>

#include <sel4bench/sel4bench.h>
#include <sel4platsupport/timer.h>
#include <sel4platsupport/io.h>
#include <sel4rpc/client.h>
#include <sel4runtime.h>
#include <sel4utils/helpers.h>
#include <sel4utils/process.h>
#include <serial_server/client.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <arch_stdio.h>
#include <string.h>
#include <rpc.pb.h>
#include <libfdt.h>

#include <benchmark.h>

#include <utils/util.h>
#include <sel4/sel4.h>

#include "support.h"

/* benchmarking environment */
static env_t env = {{0}};

/* dummy globals for libsel4muslcsys */
char _cpio_archive[1];
char _cpio_archive_end[1];

/* so malloc works */
#define MORE_CORE_SIZE 0x300000
extern char *morecore_area;
extern size_t morecore_size;
static char ALIGN(0x1000) app_morecore_area[MORE_CORE_SIZE];

/* allocator */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];
#define ALLOCMAN_VIRTUAL_SIZE BIT(20)

/* early processing helper function */
ccnt_t getMinOverhead(ccnt_t *overhead, seL4_Word overhead_size)
{
    ccnt_t min = -1;

    for (int i = 0; i < overhead_size; i++) {
        min = (overhead[i] < min) ? overhead[i] : min;
    }

    return min;
}

/* serial server */
static serial_client_context_t context;

size_t benchmark_write(void *data, size_t count)
{
    return (size_t) serial_server_write(&context, data, count);
}

static void parse_code_region(sel4utils_elf_region_t *region)
{
    extern char __executable_start[];
    extern char _etext[];

    region->rights = seL4_AllRights;
    region->reservation_vstart = (void *) ROUND_DOWN((seL4_Word) __executable_start, (1 << seL4_PageBits));
    region->size = (void *) _etext - region->reservation_vstart;
    region->elf_vstart = region->reservation_vstart;
    region->cacheable = true;
}

static size_t add_frames(void *frames[], size_t start, uintptr_t addr, size_t num_frames)
{
    size_t i;
    for (i = start; i < (num_frames + start); i++) {
        frames[i] = (void *) addr;
        addr += SIZE_BITS_TO_BYTES(seL4_PageBits);
    }

    return i;
}

static allocman_t *init_allocator(simple_t *simple, vka_t *vka)
{
    /* set up malloc area */
    morecore_area = app_morecore_area;
    morecore_size = MORE_CORE_SIZE;

    /* set up allocator */
    allocman_t *allocator = bootstrap_new_1level_simple(simple, CONFIG_SEL4UTILS_CSPACE_SIZE_BITS,
                                                        ALLOCATOR_STATIC_POOL_SIZE,
                                                        allocator_mem_pool);

    ZF_LOGF_IF(!allocator, "Failed to initialize allocator");
    /* create vka backed by allocator */
    allocman_make_vka(vka, allocator);
    return allocator;
}

static void init_allocator_vspace(allocman_t *allocator, vspace_t *vspace)
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

static void init_vspace(vka_t *vka, vspace_t *vspace, sel4utils_alloc_data_t *data,
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

static seL4_Error get_irq(void *data, int irq, seL4_CNode cnode, seL4_Word index, uint8_t depth)
{
    env_t *env = data;

    RpcMessage msg = {
        .which_msg = RpcMessage_irq_tag,
        .msg.irq = {
            .which_type = IrqAllocMessage_simple_tag,
            .type.simple = {
                .setTrigger = false,
                .irq = irq,
            },
        },
    };

    int ret = sel4rpc_call(&env->rpc_client, &msg, cnode, index, depth);
    if (ret) {
        return ret;
    }

    return msg.msg.ret.errorCode;
}

static inline sel4utils_process_config_t get_process_config(env_t *env, uint8_t prio, void *entry_point)
{
    sel4utils_process_config_t config = process_config_new(&env->simple);
    config = process_config_noelf(config, entry_point, 0);
    config = process_config_create_cnode(config, CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    config = process_config_create_vspace(config, &env->region, 1);
    config = process_config_priority(config, prio);
#ifdef CONFIG_KERNEL_MCS
    config.sched_params = sched_params_round_robin(config.sched_params, &env->simple, 0,
                                                   CONFIG_BOOT_THREAD_TIME_SLICE * NS_IN_US);
#endif
    return process_config_mcp(config, prio);
}

void benchmark_shallow_clone_process(env_t *env, sel4utils_process_t *process, uint8_t prio, void *entry_point,
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

void benchmark_configure_thread_in_process(env_t *env, sel4utils_process_t *process,
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

void benchmark_configure_thread(env_t *env, seL4_CPtr fault_ep, uint8_t prio, char *name, sel4utils_thread_t *thread)
{
    sel4utils_thread_config_t config = thread_config_new(&env->simple);
    config = thread_config_fault_endpoint(config, fault_ep);
    config = thread_config_priority(config, prio);
    config = thread_config_mcp(config, prio);
    config = thread_config_auth(config, simple_get_tcb(&env->simple));
#ifdef CONFIG_KERNEL_MCS
    config.sched_params = sched_params_round_robin(config.sched_params, &env->simple, 0,
                                                   CONFIG_BOOT_THREAD_TIME_SLICE * NS_IN_US);
#endif
    config = thread_config_create_reply(config);
    int error = sel4utils_configure_thread_config(&env->slab_vka, &env->vspace, &env->vspace, config, thread);
    ZF_LOGF_IF(error, "Failed to configure %s\n", name);
    NAME_THREAD(thread->tcb.cptr, name);
}

void benchmark_wait_children(seL4_CPtr ep, char *name, int num_children)
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

/*
 * We use sel4runtime to create the TLS for the thread as though it
 * were in our address space and then copy the contents into the
 * remote address space.
 *
 * This causes a few fields (the TLS references to itself and the
 * other TLS data) to use the incorrect addresses.
 *
 * Only the reference to self needs to be recorded correctly, so we
 * calculate what the value should be for the remote address space
 * before copying it along with the rest of the TLS into place.
 *
 * We then determine the tp value for the remote address space before
 * and set the TLS base before starting the process as normal.
 */
int benchmark_spawn_process(sel4utils_process_t *process, vka_t *vka, vspace_t *vspace, int argc,
                            char *argv[], int resume)
{
    size_t tls_size = sel4runtime_get_tls_size();
    if (tls_size > PAGE_SIZE_4K) {
        ZF_LOGE("TLS requires more than one page. (%zu/%zu)", tls_size, PAGE_SIZE_4K);
        return -1;
    }

    // Create local copy of TLS, limited to 4K.
    unsigned char local_tls[PAGE_SIZE_4K] = {};
    uintptr_t local_tp = (uintptr_t)sel4runtime_write_tls_image(&local_tls);
    size_t tp_offset = local_tp - (uintptr_t) &local_tls[0];
    sel4runtime_set_tls_variable(
        local_tp,
        __sel4_ipc_buffer,
        (seL4_IPCBuffer *)process->thread.ipc_buffer_addr
    );

    // Get the relevant addresses for the destination.
    uintptr_t initial_stack_pointer = (uintptr_t)process->thread.stack_top - sizeof(seL4_Word);
    uintptr_t dest_tls_addr = initial_stack_pointer - tls_size;
    uintptr_t dest_tp = dest_tls_addr + tp_offset;

#if defined(CONFIG_ARCH_X86)
    // Overwrite the self reference for x86 (needed to dereference TLS).
    *(void **)(local_tp) = (void *) dest_tp;
#endif

    // Copy TLS onto thread stack.
    int error = sel4utils_stack_write(
                    vspace,
                    &process->vspace,
                    vka,
                    local_tls,
                    tls_size,
                    &initial_stack_pointer
                );
    if (error) {
        return -1;
    }

    assert(dest_tls_addr == initial_stack_pointer);

    // Save the initial stack top.
    void *initial_stack_top = process->thread.stack_top;

    /*
     * Move the stack pointer down to a place sel4utils_spawn_process can write to.
     * To be compatible with as many architectures as possible
     * we need to ensure double word alignment.
     */
    process->thread.stack_top = (void *) ALIGN_DOWN(initial_stack_pointer - sizeof(seL4_Word), STACK_CALL_ALIGNMENT);

    error = sel4utils_spawn_process(process, vka, vspace, argc, argv, false);
    if (error) {
        return error;
    }

    // Restore the initial stack top.
    process->thread.stack_top = initial_stack_top;

    // Configure TLS base for the thread.
    error = seL4_TCB_SetTLSBase(process->thread.tcb.cptr, dest_tp);
    if (error) {
        return error;
    }

    if (resume) {
        error = seL4_TCB_Resume(process->thread.tcb.cptr);
    }

    return error;
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

static int get_cap_count(void *data)
{
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

    if (!config_set(CONFIG_KERNEL_MCS)) {
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
        if (!config_set(CONFIG_KERNEL_MCS) && n >= SEL4UTILS_SCHED_CONTEXT_SLOT) {
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

static ssize_t extended_bootinfo_len(void *data, seL4_Word type)
{
    /* This is mainly used to suppress warnings about how the original function was not implemented. */
    return -1;
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
    env->simple.extended_bootinfo_len = extended_bootinfo_len;

    env->simple.arch_simple.data = env;
    env->simple.arch_simple.irq = get_irq;
    benchmark_arch_get_simple(&env->simple.arch_simple);
}

static sel4rpc_client_t *rpc_client;
/* TODO move a method like this into libsel4rpc? */
static int benchmark_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type,
                                         seL4_Word size_bits, uintptr_t paddr, seL4_Word *cookie)
{
    RpcMessage msg = {
        .which_msg = RpcMessage_memory_tag,
        .msg.memory = {
            .address = paddr,
            .size_bits = size_bits,
            .type = type,
            .action = Action_ALLOCATE,
        },
    };

    sel4rpc_call(rpc_client, &msg, dest->root, dest->capPtr, dest->capDepth);
    *cookie = msg.msg.ret.cookie;
    return msg.msg.ret.errorCode;
}

static void benchmark_utspace_free_fn(void *data, seL4_Word type, seL4_Word size_bits, seL4_Word target)
{
    RpcMessage msg = {
        .which_msg = RpcMessage_memory_tag,
        .msg.memory = {
            .address = target,
            .size_bits = size_bits,
            .type = type,
            .action = Action_FREE,
        },
    };

    sel4rpc_call(rpc_client, &msg, seL4_CapNull, seL4_CapNull, seL4_CapNull);
}

static vka_utspace_alloc_at_fn utspace_alloc_copy;
static vka_utspace_free_fn utspace_free_copy;

void benchmark_init_timer(env_t *env)
{
    /* Copy and replace the vka utspace functions */
    utspace_alloc_copy = env->slab_vka.utspace_alloc_at;
    utspace_free_copy = env->slab_vka.utspace_free;
    env->slab_vka.utspace_alloc_at = benchmark_utspace_alloc_at_fn;
    env->slab_vka.utspace_free = benchmark_utspace_free_fn;

    rpc_client = &env->rpc_client;

    int error = ltimer_default_init(&env->ltimer, env->io_ops, NULL, NULL);
    ZF_LOGF_IF(error, "Failed to init timer");

    /* Restore the vka utspace functions */
    env->slab_vka.utspace_alloc_at = utspace_alloc_copy;
    env->slab_vka.utspace_free = utspace_free_copy;

    env->timer_initialised = true;
}

NORETURN void benchmark_finished(int exit_code)
{
    /* stop the timer */
    if (env.timer_initialised) {
        /* overwrite parts of the VKA again so that it can free the resources */
        env.slab_vka.utspace_free = benchmark_utspace_free_fn;

        ltimer_destroy(&env.ltimer);
    }

    /* stop the serial */
    serial_server_disconnect(&context);

    /* send back exit code */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
    seL4_SetMR(0, exit_code);
    seL4_Call(SEL4UTILS_ENDPOINT_SLOT, info);
    while (true);
}

static char *benchmark_io_fdt_get(void *cookie)
{
    return (cookie != NULL) ? (char *) cookie : NULL;
}

static int benchmark_setup_io_fdt(ps_io_fdt_t *io_fdt, char *fdt_blob)
{
    if (!io_fdt || !fdt_blob) {
        ZF_LOGE("Args are NULL");
        return -1;
    }

    io_fdt->cookie = fdt_blob;
    io_fdt->get_fn = benchmark_io_fdt_get;

    return 0;
}

env_t *benchmark_get_env(int argc, char const *const *argv, size_t results_size,
                         size_t object_freq[seL4_ObjectTypeCount])
{
    sel4muslcsys_register_stdio_write_fn(benchmark_write);

    /* parse arguments */
    if (argc < 1) {
        ZF_LOGF("Insufficient arguments, expected 1, got %d\n", (int) argc);
    }

    env.args = (void *) atol(argv[0]);
    env.results = env.args->results;
    init_simple(&env);

    sel4rpc_client_init(&env.rpc_client, SEL4UTILS_ENDPOINT_SLOT, SEL4BENCH_PROTOBUF_RPC);
    env.allocman = init_allocator(&env.simple, &env.delegate_vka);
    init_vspace(&env.delegate_vka, &env.vspace, &env.data, env.args->stack_pages, env.args->stack_vaddr,
                (uintptr_t) env.results, results_size, (uintptr_t) env.args);
    init_allocator_vspace(env.allocman, &env.vspace);
    parse_code_region(&env.region);

    /* set up serial */
    int error = serial_server_client_connect(env.args->serial_ep, &env.delegate_vka, &env.vspace, &context);
    ZF_LOGF_IF(error, "Failed to set up serial");

    /* In case we used any FPU during our setup we will attempt to put the system
     * back into a steady state before returning */
#ifdef CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH
    for (int i = 0; i < CONFIG_X86_FPU_MAX_RESTORES_SINCE_SWITCH; i++) {
        seL4_Yield();
    }
#endif

    /* update object freq for timer objects */
    object_freq[seL4_NotificationObject]++;

    /* set up slab allocator */
    if (slab_init(&env.slab_vka, &env.delegate_vka, object_freq) != 0) {
        ZF_LOGF("Failed to init slab allocator");
    }

    /* allocate a notification for timers */
    error = vka_alloc_notification(&env.slab_vka, &env.ntfn);
    ZF_LOGF_IF(error, "Failed to allocate ntfn");

    /* initialise a ps_io_ops_t structure, individually as we don't want to
     * use the default IRQ interface */
    error = sel4platsupport_new_malloc_ops(&env.io_ops.malloc_ops);
    ZF_LOGF_IF(error, "Failed to setup the malloc interface");

    error = sel4platsupport_new_io_mapper(&env.vspace, &env.slab_vka, &env.io_ops.io_mapper);
    ZF_LOGF_IF(error, "Failed to setup the IO mapper");

    if (env.args->fdt) {
        /* manually setup the fdt interface */
        error = benchmark_setup_io_fdt(&env.io_ops.io_fdt, env.args->fdt);
        ZF_LOGF_IF(error, "Failed to setup the FDT interface");
    }

    error = sel4platsupport_new_arch_ops(&env.io_ops, &env.simple, &env.slab_vka);
    ZF_LOGF_IF(error, "Failed to setup the arch ops");

    error = sel4platsupport_new_mini_irq_ops(&env.io_ops.irq_ops, &env.slab_vka, &env.simple, &env.io_ops.malloc_ops,
                                             env.ntfn.cptr, MASK(seL4_BadgeBits));
    ZF_LOGF_IF(error, "Failed to setup the mini IRQ interface");

    env.ntfn_id = MINI_IRQ_INTERFACE_NTFN_ID;

    return &env;
}

void send_result(seL4_CPtr ep, ccnt_t result)
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

ccnt_t get_result(seL4_CPtr ep)
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
