/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

/* Include Kconfig variables. */
#include <autoconf.h>
#include <sel4benchapp/gen_config.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <assert.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <sel4debug/register_dump.h>
#include <serial_server/parent.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/timer.h>
#include <sel4rpc/server.h>
#include <sel4utils/api.h>
#include <sel4utils/stack.h>
#include <stdio.h>
#include <string.h>
#include <utils/util.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <libfdt.h>

#include <ipc.h>
#include <benchmark_types.h>

#include "benchmark.h"
#include "env.h"
#include "printing.h"
#include "processing.h"

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 200)

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 100)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* static memory for virtual memory bootstrapping */
static sel4utils_alloc_data_t data;

/* environment for the benchmark runner, set up in main() */
static env_t global_env;

static void setup_fault_handler(env_t *env)
{
    int error;
    sel4utils_thread_t fault_handler;
    vka_object_t fault_ep = {0};

    printf("Setting up global fault handler...\n");

    /* create an endpoint */
    if (vka_alloc_endpoint(&env->vka, &fault_ep) != 0) {
        ZF_LOGF("Failed to create fault ep\n");
    }

    /* set the fault endpoint for the current thread */
    error = api_tcb_set_space(simple_get_tcb(&env->simple), fault_ep.cptr,
                              simple_get_cnode(&env->simple), seL4_NilData, simple_get_pd(&env->simple),
                              seL4_NilData);
    ZF_LOGF_IFERR(error, "Failed to set fault ep for benchmark driver\n");

    error = sel4utils_start_fault_handler(fault_ep.cptr,
                                          &env->vka, &env->vspace, simple_get_cnode(&env->simple),
                                          seL4_NilData,
                                          "sel4bench-fault-handler", &fault_handler);
    ZF_LOGF_IFERR(error, "Failed to start fault handler");

    error = seL4_TCB_SetPriority(fault_handler.tcb.cptr, simple_get_tcb(&env->simple), seL4_MaxPrio);
    ZF_LOGF_IFERR(error, "Failed to set prio for fault handler");

    if (config_set(CONFIG_KERNEL_MCS)) {
        /* give it a sc */
        error = vka_alloc_sched_context(&env->vka, &fault_handler.sched_context);
        ZF_LOGF_IF(error, "Failed to allocate sc");

        error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0),
                                         fault_handler.sched_context.cptr,
                                         100 * US_IN_S, 100 * US_IN_S, 0, 0);
        ZF_LOGF_IF(error, "Failed to configure sc");

        error = api_sc_bind(fault_handler.sched_context.cptr, fault_handler.tcb.cptr);
        ZF_LOGF_IF(error, "Failed to bind sc to fault handler");
    }
}

int run_benchmark(env_t *env, benchmark_t *benchmark, void *local_results_vaddr, benchmark_args_t *args)
{
    int error;
    sel4utils_process_t process;
    sel4rpc_server_env_t rpc_env;

    /* configure benchmark process */
    sel4utils_process_config_t config = process_config_default_simple(&env->simple, benchmark->name,
                                                                      seL4_MaxPrio);
    config = process_config_mcp(config, seL4_MaxPrio);
    error = sel4utils_configure_process_custom(&process, &env->vka, &env->vspace, config);
    ZF_LOGF_IFERR(error, "Failed to configure process for %s benchmark", benchmark->name);

    /* initialise sched ctrl for benchmark environment */
    if (config_set(CONFIG_KERNEL_MCS)) {
        seL4_CPtr sched_ctrl = simple_get_sched_ctrl(&env->simple, 0);
        args->sched_ctrl = sel4utils_copy_cap_to_process(&process, &env->vka, sched_ctrl);
        for (int i = 1; i < CONFIG_MAX_NUM_NODES; i++) {
            sched_ctrl = simple_get_sched_ctrl(&env->simple, i);
            sel4utils_copy_cap_to_process(&process, &env->vka, sched_ctrl);
        }
    }

    /* copy serial to process */
    args->serial_ep = serial_server_parent_mint_endpoint_to_process(&process);
    ZF_LOGF_IF(args->serial_ep == 0, "Failed to copy rpc serial ep to process");

    /* copy untyped to process */
    args->untyped_cptr = sel4utils_copy_cap_to_process(&process, &env->vka, env->untyped.cptr);
    /* this is the last cap we copy - initialise the first free cap */
    args->first_free = args->untyped_cptr + 1;

    args->stack_pages = CONFIG_SEL4UTILS_STACK_SIZE / SIZE_BITS_TO_BYTES(seL4_PageBits);
    args->stack_vaddr = ((uintptr_t) process.thread.stack_top) - CONFIG_SEL4UTILS_STACK_SIZE;

    NAME_THREAD(process.thread.tcb.cptr, benchmark->name);

    /* set up shared memory for results */
    args->results = vspace_share_mem(&env->vspace, &process.vspace, local_results_vaddr,
                                     benchmark->results_pages, seL4_PageBits, seL4_AllRights, true);

    /* do benchmark specific init */
    benchmark->init(&env->vka, &env->simple, &process);

    size_t num_fdt_pages = 0;
    if (config_set(CONFIG_ARCH_ARM)) {
        char *fdt_blob = ps_io_fdt_get(&env->ops.io_fdt);
        ZF_LOGF_IF(!fdt_blob, "Failed to get the FDT blob for sharing with the benchmark process");
        size_t fdt_size = fdt_totalsize(fdt_blob);
        /* perhaps optimise to use larger page sizes if possible? */
        num_fdt_pages = DIV_ROUND_UP(fdt_size, BIT(seL4_PageBits));
        /* share the FDT with the benchmarking process */
        args->fdt = vspace_share_mem(&env->vspace, &process.vspace, fdt_blob, num_fdt_pages,
                                     seL4_PageBits, seL4_AllRights, true);
        ZF_LOGF_IF(!args->fdt, "Failed to share the FDT blob");
        /* offset the shared address if the address is page aligned */
        if (((uintptr_t) args->fdt & MASK(seL4_PageBits)) != ((uintptr_t) fdt_blob & MASK(seL4_PageBits))) {
            args->fdt += (uintptr_t) fdt_blob & MASK(seL4_PageBits);
        }
    }

    /* set up arguments */
    void *remote_args_vaddr = vspace_share_mem(&env->vspace, &process.vspace, args, 1,
                                               seL4_PageBits, seL4_AllRights, true);
    args->untyped_size_bits = env->untyped.size_bits;
    args->nr_cores = simple_get_core_count(&env->simple);

    /* set up rpc server environment */
    error = sel4rpc_server_init(&rpc_env, &env->vka, sel4rpc_default_handler, env, &process.thread.reply,
                                &env->simple);
    ZF_LOGF_IF(error, "Failed to initialise RPC server environment");

    /* untyped size */
    seL4_Word argc = 1;
    char string_args[argc][WORD_STRING_SIZE];
    char *argv[argc];
    sel4utils_create_word_args(string_args, argv, argc, remote_args_vaddr);
    /* start process */
    error = sel4utils_spawn_process_v(&process, &env->vka, &env->vspace, argc, argv, 1);
    ZF_LOGF_IF(error, "Failed to start benchmark process");

    /* wait for it to finish */
    int result = SEL4BENCH_PROTOBUF_RPC;
    while (result == SEL4BENCH_PROTOBUF_RPC) {
        seL4_MessageInfo_t info = api_recv(process.fault_endpoint.cptr, NULL, process.thread.reply.cptr);
        result = seL4_GetMR(0);
        if (seL4_MessageInfo_get_label(info) != seL4_Fault_NullFault) {
            sel4utils_print_fault_message(info, benchmark->name);
            sel4debug_dump_registers(process.thread.tcb.cptr);
            result = EXIT_FAILURE;
        } else if (result == SEL4BENCH_PROTOBUF_RPC) {
            sel4rpc_server_recv(&rpc_env);
        } else if (result != EXIT_SUCCESS) {
            ZF_LOGE("Benchmark failed, result %d\n", result);
            sel4debug_dump_registers(process.thread.tcb.cptr);
        }
    }

    /* free results in target vspace (they will still be in ours) */
    vspace_unmap_pages(&process.vspace, args->results, benchmark->results_pages, seL4_PageBits, VSPACE_FREE);
    vspace_unmap_pages(&process.vspace, remote_args_vaddr, 1, seL4_PageBits, VSPACE_FREE);
    if (config_set(CONFIG_ARCH_ARM)) {
        /* free the shared FDT, align it just in case we've offsetted the addr */
        void *aligned_fdt_addr = (void *) ALIGN_DOWN((uintptr_t) args->fdt, BIT(seL4_PageBits));
        vspace_unmap_pages(&process.vspace, aligned_fdt_addr, num_fdt_pages, seL4_PageBits, VSPACE_FREE);
    }
    /* clean up */

    /* revoke the untypeds so it's clean for the next benchmark */
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, env->untyped.cptr, &path);
    vka_cnode_revoke(&path);

    /* destroy the process */
    sel4utils_destroy_process(&process, &env->vka);

    return result;
}

json_t *launch_benchmark(benchmark_t *benchmark, env_t *env)
{
    printf("\n%s Benchmarks\n==============\n\n", benchmark->name);

    /* reserve memory for the results */
    void *results = vspace_new_pages(&env->vspace, seL4_AllRights, benchmark->results_pages, seL4_PageBits);
    ZF_LOGF_IF(results == NULL, "Failed to allocate pages for results");

    /* reserve memory for args */
    assert(sizeof(benchmark_args_t) < PAGE_SIZE_4K);
    void *args = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    /* Run benchmark process */
    int exit_code = run_benchmark(env, benchmark, results, args);

    /* process & print results */
    json_t *json = NULL;
    if (exit_code == EXIT_SUCCESS) {
        json = benchmark->process(results);
    }

    /* free results */
    vspace_unmap_pages(&env->vspace, results, benchmark->results_pages, seL4_PageBits, VSPACE_FREE);
    vspace_unmap_pages(&env->vspace, args, 1, seL4_PageBits, VSPACE_FREE);

    return json;
}

void find_untyped(vka_t *vka, vka_object_t *untyped)
{
    int error = 0;

    /* find  a large untyped */
    for (uint8_t size_bits = seL4_MaxUntypedBits; size_bits > seL4_PageBits; size_bits--) {
        error = vka_alloc_untyped(vka, size_bits, untyped);
        if (error == 0) {
            break;
        }
    }
    ZF_LOGF_IF(error, "Failed to find free untyped\n");
}

void *main_continued(void *arg)
{

    setup_fault_handler(&global_env);

    /* find an untyped for the process to use */
    find_untyped(&global_env.vka, &global_env.untyped);

    /* list of benchmarks */
    benchmark_t *benchmarks[] = {
        ipc_benchmark_new(),
        irq_benchmark_new(),
        irquser_benchmark_new(),
        scheduler_benchmark_new(),
        signal_benchmark_new(),
        fault_benchmark_new(),
        hardware_benchmark_new(),
        sync_benchmark_new(),
        /* add new benchmarks here */
        page_mapping_benchmark_new(),
        smp_benchmark_new(),
        vcpu_benchmark_new(),

        /* null terminator */
        NULL
    };

    json_t *output = json_array();
    assert(output != NULL);
    int error;

    /* run the benchmarks */
    for (int i = 0; benchmarks[i] != NULL; i++) {
        if (benchmarks[i]->enabled) {
            json_t *result = launch_benchmark(benchmarks[i], &global_env);
            ZF_LOGF_IF(result == NULL, "Failed to run benchmark %s", benchmarks[i]->name);
            error = json_array_extend(output, result);
            ZF_LOGF_IF(error != 0, "Failed to add benchmark results");
        }
    }

    printf("JSON OUTPUT\n");
    error = json_dumpf(output, stdout, JSON_PRESERVE_ORDER | JSON_INDENT(CONFIG_JSON_INDENT));
    ZF_LOGF_IF(error, "Failed to dump output");

    printf("END JSON OUTPUT\n");
    printf("All is well in the universe.\n");
    printf("\n\nFin\n");

    return 0;
}

/* globals for malloc */
extern vspace_t *muslc_this_vspace;
extern reservation_t muslc_brk_reservation;
extern void *muslc_brk_reservation_start;

int main(void)
{
    seL4_BootInfo *info;
    allocman_t *allocman;
    UNUSED reservation_t virtual_reservation;
    UNUSED int error;
    void *vaddr;

    info  = platsupport_get_bootinfo();
    simple_default_init_bootinfo(&global_env.simple, info);

    /* create allocator */
    allocman = bootstrap_use_current_simple(&global_env.simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    assert(allocman);

    /* create vka */
    allocman_make_vka(&global_env.vka, allocman);

    /* create vspace */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&global_env.vspace, &data, simple_get_pd(&global_env.simple),
                                                           &global_env.vka, info);

    /* set up malloc */
    sel4utils_res_t malloc_res;
    error = sel4utils_reserve_range_no_alloc(&global_env.vspace, &malloc_res, seL4_LargePageBits, seL4_AllRights, 1,
                                             &muslc_brk_reservation_start);
    muslc_this_vspace = &global_env.vspace;
    muslc_brk_reservation.res = &malloc_res;
    ZF_LOGF_IF(error, "Failed to set up dynamic malloc");

    virtual_reservation = vspace_reserve_range(&global_env.vspace, ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights,
                                               1, &vaddr);
    assert(virtual_reservation.res);
    bootstrap_configure_virtual_pool(allocman, vaddr, ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&global_env.simple));

    /* init serial */
    platsupport_serial_setup_simple(&global_env.vspace, &global_env.simple, &global_env.vka);

    error = serial_server_parent_spawn_thread(&global_env.simple, &global_env.vka,
                                              &global_env.vspace, seL4_MaxPrio);
    ZF_LOGF_IF(error, "Failed to start serial server");

    if (config_set(CONFIG_ARCH_ARM)) {
        /* initialise the FDT interface while we have access to the bootinfo */
        error = sel4platsupport_new_malloc_ops(&global_env.ops.malloc_ops);
        ZF_LOGF_IF(error, "Failed to get malloc_ops");
        error = sel4platsupport_new_fdt_ops(&global_env.ops.io_fdt, &global_env.simple, &global_env.ops.malloc_ops);
        ZF_LOGF_IF(error, "Failed to get fdt ops");
    }

    /* Print welcome banner. */
    printf("\n");
    printf("%s%sse%sL4%s Benchmark%s\n", A_BOLD, A_FG_W, A_FG_G, A_FG_W, A_RESET);
    printf("%s==============%s\n", A_FG_G, A_FG_W);
    printf("\n");

    if (plat_setup) {
        plat_setup(&global_env);
    }

    /* Switch to a bigger stack with a guard page! */
    printf("Switching to a safer, bigger stack... \n");
    error = sel4utils_run_on_stack(&global_env.vspace, main_continued, NULL, NULL);
    assert(error == 0);

    return 0;
}
