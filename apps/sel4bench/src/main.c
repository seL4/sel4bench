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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sel4platsupport/platsupport.h>

#include <twinkle/allocator.h>
#include <twinkle/bootstrap.h>
#include <twinkle/vka.h>
#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
#include <simple-stable/simple-stable.h>
#else
#include <simple-default/simple-default.h>
#endif

#include <vka/object.h>

#include "ipc.h"
#include "helpers.h"
#include "test.h"
#include "util/ansi.h"

static vka_t global_vka;
static simple_t global_simple;
static struct allocator *global_allocator;
static struct allocator _pertest_allocator;
static struct allocator *pertest_allocator = &_pertest_allocator;
static helper_thread_t global_fault_handler_thread;
static seL4_CPtr roottask_fault_handler_ep;
seL4_CPtr global_fault_handler_ep;
struct env global_environment;

static void
setup_fault_handler(void);

/* Setup a test environment. */
static void
setup_environment(struct env *env)
{
    env->allocator = pertest_allocator;
    env->simple = &global_simple;
    twinkle_init_vka(&env->vka, env->allocator);
}

/* Cleanup a test environment. */
static void
cleanup_environment(struct env *env)
{
    allocator_reset(env->allocator);
}

static void
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

void *main_continued(void *arg);
int main(void)
{
    global_allocator = create_first_stage_allocator();
    twinkle_init_vka(&global_vka, global_allocator);
    
#ifdef CONFIG_KERNEL_STABLE
    simple_stable_init_bootinfo(&global_simple, seL4_GetBootInfo());
#else
    simple_default_init_bootinfo(&global_simple, seL4_GetBootInfo());
#endif
    platsupport_serial_setup_simple(NULL, &global_simple, &global_vka);
    /* Print welcome banner. */
    printf("\n");
    printf("%s%sse%sL4%s Benchmark%s\n", A_BOLD, A_FG_W, A_FG_G, A_FG_W, A_RESET);
    printf("%s==============%s\n", A_FG_G, A_FG_W);
    printf("\n");

    check_cpu_features();

    /* Switch to a bigger stack with some guard pages! */
    printf("Switching to a safer, bigger stack... ");
    return (int)run_on_stack(&global_vka, 16, main_continued, NULL);
}

void *main_continued(void *arg)
{
    printf("done\n");

    setup_fault_handler();

    printf("Setup timing...");
    timing_init();
    printf(" done\n");

    /* Setup an allocator for ourself. */
    printf("Setting up per-test allocator... ");
    allocator_create_child(global_allocator, pertest_allocator,
            global_allocator->root_cnode,
            global_allocator->root_cnode_depth,
            global_allocator->root_cnode_offset,
            global_allocator->cslots.first + global_allocator->num_slots_used,
            global_allocator->cslots.count - global_allocator->num_slots_used);
    printf("done\n");

    printf("Setting up benchmarking environment...");
    setup_environment(&global_environment);
    printf(" done\n");

    /* Run the benchmarking */
    printf("Doing benchmarks...\n\n");
    const timing_print_mode_t print_mode = timing_print_formatted;
    ipc_benchmarks_new(&global_environment, print_mode);
    cleanup_environment(&global_environment);
    printf("All is well in the universe.\n");
    printf("\n\nFin\n");

    return 0;
}

static void
setup_fault_handler(void)
{
    /* Create a thread to handle faults. */
    printf("Setting up global fault handler...");
    global_fault_handler_ep = vka_alloc_endpoint_leaky(&global_vka);
    create_helper_thread(&global_fault_handler_thread,
            &global_vka,
            (helper_func_t)fault_handler_thread, 0,
            global_fault_handler_ep, 0, 0, 0);
    start_helper_thread(&global_fault_handler_thread);

    /* Make an endpoint just for the root task. */
    int error = vka_cspace_alloc(&global_vka, &roottask_fault_handler_ep);
    (void)error;
    assert(!error);

    seL4_CapData_t cap_data;
    cap_data = seL4_CapData_Badge_new(ROOTTASK_FAULT_BADGE);
    error = seL4_CNode_Mint(
                seL4_CapInitThreadCNode, roottask_fault_handler_ep, seL4_WordBits,
                seL4_CapInitThreadCNode, global_fault_handler_ep, seL4_WordBits,
                seL4_AllRights, cap_data);
    assert(!error);

    error = seL4_TCB_SetSpace(seL4_CapInitThreadTCB,
            roottask_fault_handler_ep,
            seL4_CapInitThreadCNode, seL4_NilData,
            seL4_CapInitThreadPD, seL4_NilData);
    assert(!error);
#ifdef CONFIG_X86_64
    printf(" on ep %lld done\n", roottask_fault_handler_ep);
#else
    printf(" on ep %d done\n", roottask_fault_handler_ep);
#endif
}
