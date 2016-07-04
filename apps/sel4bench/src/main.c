/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

/* Include Kconfig variables. */
#include <autoconf.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <assert.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <sel4debug/register_dump.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4utils/stack.h>
#include <stdio.h>
#include <string.h>
#include <utils/util.h>
#include <vka/object.h>
#include <vka/capops.h>

#include <cspace.h>
#include <ipc.h>

#include "benchmark.h"
#include "printing.h"
#include "processing.h"

/* Contains information about the benchmark environment. */
static struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    simple_t simple;
    vspace_t vspace;
} global_env;

typedef struct env env_t;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 100)

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
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
        ZF_LOGF("CPU does not support rdtscp instruction, halting\n");
        assert(0);
    }
#endif

}

static void
setup_fault_handler(env_t *env)
{
    int error;
    sel4utils_thread_t fault_handler;
    vka_object_t fault_ep = {0};

    ZF_LOGV("Setting up global fault handler...");

    /* create an endpoint */
    if (vka_alloc_endpoint(&env->vka, &fault_ep) != 0) {
        ZF_LOGF("Failed to create fault ep\n");
    }

    /* set the fault endpoint for the current thread */
    error = seL4_TCB_SetSpace(simple_get_tcb(&env->simple), fault_ep.cptr, seL4_CapNull,
                              simple_get_cnode(&env->simple), seL4_NilData, simple_get_pd(&env->simple),
                              seL4_NilData);
    if (error != seL4_NoError) {
        ZF_LOGF("Failed to set fault ep for benchmark driver\n");
    }

    error = sel4utils_start_fault_handler(fault_ep.cptr, &env->simple,
                                          &env->vka, &env->vspace, seL4_MaxPrio, simple_get_cnode(&env->simple),
                                          seL4_NilData,
                                          "sel4bench-fault-handler", &fault_handler);
    if (error != 0) {
        ZF_LOGF("Failed to start fault handler");
    }
}

static void
init_timers(vka_t *vka, simple_t *simple, sel4utils_process_t *process)
{
    cspacepath_t path;
    UNUSED seL4_CPtr cap;
    UNUSED int error;

#ifdef CONFIG_ARCH_X86
        /* it's the hpet, use MSIs */
        ZF_LOGF_IFERR(sel4platsupport_copy_msi_cap(vka, simple, MSI_MIN, &path),
                      "Failed to copy msi cap");
#else
        ZF_LOGF_IFERR(sel4platsupport_copy_irq_cap(vka, simple, DEFAULT_TIMER_INTERRUPT, &path),
                      "Failed to get timer interrupt");
#endif

    cap = sel4utils_move_cap_to_process(process, path, vka);
    assert(cap == TIMEOUT_TIMER_IRQ_SLOT);

    /* frame */
    if (DEFAULT_TIMER_PADDR != 0) {
        sel4platsupport_copy_frame_cap(vka, simple, (void *) DEFAULT_TIMER_PADDR, seL4_PageBits, &path);
        sel4utils_copy_cap_to_process(process, path);
    } else {
        process->cspace_next_free++;
    }

    assert(process->cspace_next_free == TIMEOUT_TIMER_FRAME_SLOT + 1);

    seL4_Word clock_timer_paddr = 0;
#ifdef  CLOCK_TIMER_PADDR
    /* this ifdef be removed once all platforms define CLOCK_TIMER_PADDR */
    clock_timer_paddr = CLOCK_TIMER_PADDR;
#endif /* CLOCK_TIMER_PADDR */

    /* some platforms don't have a physical frame for their clock timer impl */
    if (clock_timer_paddr) {
        error = sel4platsupport_copy_frame_cap(vka, simple, (void *) clock_timer_paddr,
                                             seL4_PageBits, &path);
        ZF_LOGF_IF(error != 0, "Failed to copy timer cap");
        sel4utils_move_cap_to_process(process, path, vka);
    } else {
        process->cspace_next_free++;
    }
    assert(process->cspace_next_free - 1 == CLOCK_FRAME_SLOT);

    seL4_Word clock_timer_irq = 0;
#ifdef CLOCK_TIMER_INTERRUPT
    clock_timer_irq = CLOCK_TIMER_INTERRUPT;
#endif /* CLOCK_TIMER_INTERRUPT */

    if (clock_timer_irq != 0) {
        error = sel4platsupport_copy_irq_cap(vka, simple, clock_timer_irq, &path);
        ZF_LOGF_IF(error != 0, "Failed to copy irq cap");
        sel4utils_move_cap_to_process(process, path, vka);
    } else {
        process->cspace_next_free++;
    }

    assert(process->cspace_next_free == FREE_SLOT);
}

int
run_benchmark(env_t *env, benchmark_t *benchmark, vka_object_t *untyped, void *local_results_vaddr)
{
    int error;
    sel4utils_process_t process;

    /* configure benchmark process */
    error = sel4utils_configure_process(&process, &env->simple, &env->vka, &env->vspace, seL4_MaxPrio, benchmark->name);
    if (error != 0) {
        ZF_LOGF("Failed to configure process for %s benchmark", benchmark->name);
    }

    /* copy untyped to process */
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, untyped->cptr, &path);
    UNUSED seL4_CPtr slot = sel4utils_copy_cap_to_process(&process, path);
    assert(slot == UNTYPED_SLOT);

    /* copy sched ctrl to process */
    vka_cspace_make_path(&env->vka, simple_get_sched_ctrl(&env->simple), &path);
    slot = sel4utils_copy_cap_to_process(&process, path);
    assert(slot == SCHED_CTRL_SLOT);

    seL4_Word stack_pages = CONFIG_SEL4UTILS_STACK_SIZE / SIZE_BITS_TO_BYTES(seL4_PageBits);
    uintptr_t stack_vaddr = ((uintptr_t) process.thread.stack_top) - CONFIG_SEL4UTILS_STACK_SIZE;


#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(process.thread.tcb.cptr, benchmark->name);
#endif

    /* set up shared memory for results */
    void *remote_results_vaddr = vspace_share_mem(&env->vspace, &process.vspace, local_results_vaddr,
                                                  benchmark->results_pages, seL4_PageBits, seL4_AllRights, true);

    /* initialise timers for benchmark environment */
    init_timers(&env->vka, &env->simple, &process);

    /* do benchmark specific init */
    benchmark->init(&env->vka, &env->simple, &process);

    /* set up arguments */
    /* untyped size */
    seL4_Word argc = 4;
    char args[4][WORD_STRING_SIZE];
    char *argv[argc];
    sel4utils_create_word_args(args, argv, argc, untyped->size_bits, stack_vaddr, stack_pages,
                               (seL4_Word) remote_results_vaddr);
    /* start process */
    error = sel4utils_spawn_process_v(&process, &env->vka, &env->vspace, argc, argv, 1);
    if (error) {
        ZF_LOGF("Failed to start benchmark process");
    }

    /* wait for it to finish */
    seL4_MessageInfo_t info = seL4_Recv(process.fault_endpoint.cptr, NULL);
    int result = seL4_GetMR(0);
    if (seL4_MessageInfo_get_label(info) != seL4_NoFault) {
        sel4utils_print_fault_message(info, benchmark->name);
        sel4debug_dump_registers(process.thread.tcb.cptr);
        result = EXIT_FAILURE;
    } else if (result != EXIT_SUCCESS) {
        ZF_LOGF("Benchmark failed, result %d\n", result);
        sel4debug_dump_registers(process.thread.tcb.cptr);
    }

    /* free results in target vspace (they will still be in ours) */
    vspace_unmap_pages(&process.vspace, remote_results_vaddr, benchmark->results_pages, seL4_PageBits, VSPACE_FREE);
     /* clean up */

    /* revoke the untyped so it's clean for the next benchmark */
    vka_cspace_make_path(&env->vka, untyped->cptr, &path);
    vka_cnode_revoke(&path);

    /* destroy the process */
    sel4utils_destroy_process(&process, &env->vka);

   return result;
}

json_t *
launch_benchmark(benchmark_t *benchmark, env_t *env, vka_object_t *untyped)
{
    ZF_LOGV("\n%s Benchmarks\n==============\n\n", benchmark->name);

    /* reserve memory for the results */
    void *results = vspace_new_pages(&env->vspace, seL4_AllRights, benchmark->results_pages, seL4_PageBits);

    if (results == NULL) {
        ZF_LOGF("Failed to allocate pages for results");
    }

    /* Run benchmark process */
    int exit_code = run_benchmark(env, benchmark, untyped, results);

    /* process & print results */
    json_t *json = NULL;
    if (exit_code == EXIT_SUCCESS) {
        json = benchmark->process(results);
    }

    /* free results */
    vspace_unmap_pages(&env->vspace, results, benchmark->results_pages, seL4_PageBits, VSPACE_FREE);

    return json;
}

void
find_untyped(vka_t *vka, vka_object_t *untyped)
{
    int error = 0;

    /* find the largest untyped */
    for (uint8_t size_bits = seL4_WordBits - 1; size_bits > seL4_PageBits; size_bits--) {
        error = vka_alloc_untyped(vka, size_bits, untyped);
        if (error == 0) {
            break;
        }
    }

    if (error != 0) {
        ZF_LOGF("Failed to find free untyped\n");
    }

}

void *
main_continued(void *arg)
{
    vka_object_t untyped;

    setup_fault_handler(&global_env);

    /* find an untyped for the process to use */
    find_untyped(&global_env.vka, &untyped);

    /* list of benchmarks */
    benchmark_t *benchmarks[] = {
        ipc_benchmark_new(),
        irq_benchmark_new(),
        irquser_benchmark_new(),
        scheduler_benchmark_new(),
        signal_benchmark_new(),
        ulscheduler_benchmark_new(),
        fault_benchmark_new(),
        /* add new benchmarks here */

        /* null terminator */
        NULL
    };

    json_t *output = json_array();
    assert(output != NULL);

    /* run the benchmarks */
    for (int i = 0; benchmarks[i] != NULL; i++) {
        if (benchmarks[i]->enabled) {
            json_t *result = launch_benchmark(benchmarks[i], &global_env, &untyped);
            if (result == NULL) {
                ZF_LOGF("Failed to run benchmark %s", benchmarks[i]->name);
            }
            UNUSED int error = json_array_extend(output, result);
            assert(error == 0);

        }
    }

    printf("JSON OUTPUT\n");
    int error = json_dumpf(output, stdout, JSON_PRESERVE_ORDER | JSON_INDENT(CONFIG_JSON_INDENT));
    if (error) {
        ZF_LOGF("Failed to dump output");
    }
    printf("END JSON OUTPUT\n");
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
    simple_default_init_bootinfo(&global_env.simple, seL4_GetBootInfo());

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

    /* Switch to a bigger stack with a guard page! */
    ZF_LOGV("Switching to a safer, bigger stack... ");
    error = sel4utils_run_on_stack(&global_env.vspace, main_continued, NULL, NULL);
    assert(error == 0);

    return 0;
}
