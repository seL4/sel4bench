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
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

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
init_allocator(allocman_t *allocator, vka_t *vka, size_t untyped_size_bits)
{
    uintptr_t paddr;
    int error;
    cspacepath_t path;

    /* set up malloc area */
    morecore_area = app_morecore_area;
    morecore_size = MORE_CORE_SIZE;

    /* set up allocator */
    allocator = bootstrap_use_current_1level(SEL4UTILS_CNODE_SLOT,
                                             CONFIG_SEL4UTILS_CSPACE_SIZE_BITS,
                                             FREE_SLOT,
                                             (1u << CONFIG_SEL4UTILS_CSPACE_SIZE_BITS),
                                             ALLOCATOR_STATIC_POOL_SIZE,
                                             allocator_mem_pool);
    /* create vka backed by allocator */
    allocman_make_vka(vka, allocator);

    /* add untyped memory */
    vka_cspace_make_path(vka, UNTYPED_SLOT, &path);
    error = allocman_utspace_add_uts(allocator, 1, &path, &untyped_size_bits, &paddr);
    if (error) {
        ZF_LOGF("Failed to add untyped to allocator");
    }
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
get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path)
{
    assert(paddr == (void *) DEFAULT_TIMER_PADDR);
    path->root = SEL4UTILS_CNODE_SLOT;
    path->capDepth = seL4_WordBits;
    path->capPtr = FRAME_SLOT;

    return seL4_NoError;
}

static seL4_Error
get_irq(void *data, int irq, seL4_CNode cnode, seL4_Word index, uint8_t depth)
{
    assert(irq == DEFAULT_TIMER_INTERRUPT);
    UNUSED seL4_Error error = seL4_CNode_Move(SEL4UTILS_CNODE_SLOT, index, depth,
                                              SEL4UTILS_CNODE_SLOT, IRQ_SLOT, seL4_WordBits);
    assert(error == seL4_NoError);

    return seL4_NoError;
}

static seL4_CPtr
get_port(void *data, uint16_t start_port, uint16_t end_port)
{
    return FRAME_SLOT;
}

env_t *
benchmark_get_env(int argc, char **argv, size_t results_size)
{
    size_t untyped_size_bits, stack_pages;
    uintptr_t stack_vaddr;

    /* parse arguments */
    if (argc < 4) {
        ZF_LOGF("Insufficient arguments, expected 4, got %d\n", (int) argc);
    }

    untyped_size_bits = (size_t) atol(argv[0]);
    stack_vaddr = (uintptr_t) atol(argv[1]);
    stack_pages = (size_t) atol(argv[2]);
    env.results = (void *) atoi(argv[3]);

    init_allocator(&env.allocman, &env.vka, untyped_size_bits);
    init_vspace(&env.vka, &env.vspace, &env.data, stack_pages, stack_vaddr,
                (uintptr_t) env.results, results_size);
    parse_code_region(&env.region);

    env.simple.frame_cap = get_frame_cap;
    env.simple.irq = get_irq;
    env.simple.IOPort_cap = get_port;

    return &env;
}

