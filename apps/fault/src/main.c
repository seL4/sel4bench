/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4benchfault/gen_config.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <utils/ud.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>

#include <benchmark.h>
#include <fault.h>

#define NOPS ""
#include <arch/fault.h>

#define N_FAULTER_ARGS 3
#define N_HANDLER_ARGS 7

static char faulter_args[N_FAULTER_ARGS][WORD_STRING_SIZE];
static char *faulter_argv[N_FAULTER_ARGS];

static sel4utils_thread_t faulter;
sel4utils_thread_t fault_handler;

char handler_args[N_HANDLER_ARGS][WORD_STRING_SIZE];
char *handler_argv[N_HANDLER_ARGS];

void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

static inline void fault(void)
{
    utils_undefined_instruction();
}

static void parse_handler_args(int argc, char **argv,
                               seL4_CPtr *ep, volatile ccnt_t **start, fault_results_t **results,
                               seL4_CPtr *done_ep, seL4_CPtr *reply, seL4_CPtr *tcb)
{
    assert(argc == N_HANDLER_ARGS);
    *ep = atol(argv[0]);
    *start = (volatile ccnt_t *) atol(argv[1]);
    *results = (fault_results_t *) atol(argv[2]);
    *done_ep = atol(argv[3]);
    *reply = atol(argv[4]);
    *tcb = atol(argv[5]);
}

static inline void fault_handler_done(seL4_CPtr ep, seL4_Word ip, seL4_CPtr done_ep, seL4_CPtr reply)
{
    /* handle last fault */
    ip += UD_INSTRUCTION_SIZE;
    seL4_ReplyWith1MR(ip, reply);
    /* tell benchmark we are done and that there are no errors */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ep, NULL);
}

static inline seL4_Word fault_handler_start(seL4_CPtr ep, seL4_CPtr done_ep, seL4_CPtr reply)
{
    seL4_Word ip;

    /* signal driver to convert us to passive and block */
    if (config_set(CONFIG_KERNEL_MCS)) {
        api_nbsend_recv(done_ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
        ip = seL4_GetMR(0);
    } else {
        /* wait for first fault */
        seL4_RecvWith1MR(ep, &ip, reply);
    }
    return ip;
}

/* Set of functions and helpers to test vm fault performance*/

#define BAD_VADDR 0x7EDCBA987650

extern char read_fault_address[];
extern char read_fault_restart_address[];

static void __attribute__((noinline)) read_fault(void)
{
    int *x = (int *)BAD_VADDR;
    /* Do a read fault. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        "read_fault_address:\n\t"
        "ldr r0, [%[addrreg]]\n\t"
        "read_fault_restart_address:\n\t"
        :
        : [addrreg] "r"(x)
        : "r0"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        "read_fault_address:\n\t"
        "ldr x0, [%[addrreg]]\n\t"
        "read_fault_restart_address:\n\t"
        :
        : [addrreg] "r"(x)
        : "x0"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        "read_fault_address:\n\t"
        LOAD_S " a0, 0(%[addrreg])\n\t"
        "read_fault_restart_address:\n\t"
        :
        : [addrreg] "r"(x)
        : "a0"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile(
        "read_fault_address:\n\t"
        "mov (%[addrreg]), %%eax\n\t"
        "read_fault_restart_address:\n\t"
        :
        : [addrreg] "r"(x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
}

static void __attribute__((noinline))
set_good_magic_and_set_pc(seL4_CPtr tcb, seL4_Word new_pc)
{
    /* Set their register to GOOD_MAGIC and set PC past fault. */
    int error;
    seL4_UserContext ctx;
    error = seL4_TCB_ReadRegisters(tcb,
                                   false,
                                   0,
                                   sizeof(ctx) / sizeof(seL4_Word),
                                   &ctx);
    assert(!error);
#if defined(CONFIG_ARCH_AARCH32)
    assert(ctx.r0 == BAD_MAGIC);
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_AARCH64)
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_RISCV)
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_X86_64)
    ctx.rip = new_pc;
#elif defined(CONFIG_ARCH_IA32)
    ctx.eip = new_pc;
#else
#error "Unknown architecture."
#endif
    error = seL4_TCB_WriteRegisters(tcb,
                                    false,
                                    0,
                                    sizeof(ctx) / sizeof(seL4_Word),
                                    &ctx);
    assert(!error);
}

static void measure_vm_fault_fn(int argc, char **argv) {
    assert (argc == N_FAULTER_ARGS);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[0]);
    seL4_CPtr done_ep = atol(argv[2]);

    for (int i = 0; i < N_RUNS + 1; i++){
        SEL4BENCH_READ_CCNT(*start);
        read_fault();
    }

    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_vm_fault_handler_fn(int argc, char **argv) {
    seL4_CPtr ep, done_ep, reply, tcb;
    volatile ccnt_t *start;
    ccnt_t end;
    fault_results_t *results;
    seL4_Word badge = 0;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);

    seL4_Word junk;

    /* signal driver to convert us to passive and block */
    if (config_set(CONFIG_KERNEL_MCS)) {
        api_nbsend_recv(done_ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
    } else {
        /* wait for first fault */
        api_recv(ep, NULL, reply);
    }

    for (int i = 0; i < N_RUNS; i++) {
        /* Clear MRs to ensure they get repopulated. */
        set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
        DO_REAL_REPLY_RECV(ep, seL4_MessageInfo_new(0, 0, 0, 0), reply);
        SEL4BENCH_READ_CCNT(end);
        results->vm_fault[i] = end - *start;

        volatile int j;
        for (j = 0; j < 10000; j++) {

        }
    }

    set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
    if (config_set(CONFIG_KERNEL_MCS)) {
        seL4_Send(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    } else {
        api_reply(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    }

    /* tell benchmark we are done and that there are no errors */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ep, NULL);
}

static void measure_vm_fault_reply_fn(int argc, char **argv) {
    assert(argc == N_FAULTER_ARGS);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[0]);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);


    /* handle 1 fault first to make sure start is set */
    read_fault();
    for (int i = 0; i < N_RUNS + 1; i++) {
        read_fault();
        ccnt_t end;
        SEL4BENCH_READ_CCNT(end);
        results->vm_fault_reply[i] = end - *start;
    }
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_vm_fault_reply_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep, reply, tcb;
    volatile ccnt_t *start;
    ccnt_t end;
    fault_results_t *results;
    seL4_Word badge = 0;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);

    /* signal driver to convert us to passive and block */
    if (config_set(CONFIG_KERNEL_MCS)) {
        api_nbsend_recv(done_ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
    } else {
        /* wait for first fault */
        api_recv(ep, NULL, reply);
    }

    for (int i = 0; i <= N_RUNS; i++) {
        set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
        /* record time */
        SEL4BENCH_READ_CCNT(*start);
        /* wait for fault */
        DO_REAL_REPLY_RECV(ep, seL4_MessageInfo_new(0, 0, 0, 0), reply);
    }

    set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
    if (config_set(CONFIG_KERNEL_MCS)) {
        seL4_Send(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    } else {
        api_reply(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    }

    /* tell benchmark we are done and that there are no errors */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ep, NULL);
}


/* Pair for measuring fault -> fault handler path */
static void measure_fault_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[0]);
    seL4_CPtr done_ep = atol(argv[2]);

    for (int i = 0; i < N_RUNS + 1; i++) {
        /* record time */
        SEL4BENCH_READ_CCNT(*start);
        fault();
    }
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_fault_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep, reply, tcb;
    volatile ccnt_t *start;
    ccnt_t end;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);
    seL4_Word ip = fault_handler_start(ep, done_ep, reply);
    for (int i = 0; i < N_RUNS; i++) {
        ip += UD_INSTRUCTION_SIZE;
        DO_REAL_REPLY_RECV_1(ep, ip, reply);
        SEL4BENCH_READ_CCNT(end);
        results->fault[i] = end - *start;
    }
    fault_handler_done(ep, ip, done_ep, reply);
}

/* Pair for measuring fault handler -> faultee path */
static void measure_fault_reply_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    volatile ccnt_t *start = (volatile ccnt_t *) atol(argv[0]);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);

    /* handle 1 fault first to make sure start is set */
    fault();
    for (int i = 0; i < N_RUNS + 1; i++) {
        fault();
        ccnt_t end;
        SEL4BENCH_READ_CCNT(end);
        results->fault_reply[i] = end - *start;
    }
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_fault_reply_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep, reply, tcb;
    volatile ccnt_t *start;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);

    seL4_Word ip = fault_handler_start(ep, done_ep, reply);
    for (int i = 0; i <= N_RUNS; i++) {
        ip += UD_INSTRUCTION_SIZE;
        /* record time */
        SEL4BENCH_READ_CCNT(*start);
        /* wait for fault */
        DO_REAL_REPLY_RECV_1(ep, ip, reply);
    }
    fault_handler_done(ep, ip, done_ep, reply);
}

/* round_trip fault handling pair */
static void measure_fault_roundtrip_fn(int argc, char **argv)
{
    assert(argc == N_FAULTER_ARGS);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);

    for (int i = 0; i < N_RUNS + 1; i++) {
        ccnt_t start, end;
        SEL4BENCH_READ_CCNT(start);
        fault();
        SEL4BENCH_READ_CCNT(end);
        results->round_trip[i] = end - start;
        volatile int j;
        for (j = 0; j < 10000; j++) {

        }
    }
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_fault_roundtrip_handler_fn(int argc, char **argv)
{
    seL4_CPtr ep, done_ep, reply, tcb;
    UNUSED volatile ccnt_t *start;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);

    seL4_Word ip = fault_handler_start(ep, done_ep, reply);
    for (int i = 0; i < N_RUNS; i++) {
        /* wait for fault */
        ip += UD_INSTRUCTION_SIZE;
        DO_REAL_REPLY_RECV_1(ep, ip, reply);
    }
    fault_handler_done(ep, ip, done_ep, reply);
}

static inline void read_mapping_fault(char *address){
    char value;
    asm volatile ("mov %[val], %[addr]"
    : [val]"=r"(value)
    : [addr]"r"(*address)
    /* no clobbers */
    );
}

static inline void write_mapping_fault(char *address){
    char value = 'a';
    asm volatile ("str %[val], [%[addr]]"
    :
    : [val]"r"(value), [addr]"r"(address)
    /* no clobbers */
    );
}

#define START_ADDR 0xA0000000

static void measure_vm_fault_map_fn(int argc, char **argv) {
    assert(argc == N_FAULTER_ARGS);
    fault_results_t *results = (fault_results_t *) atol(argv[1]);
    seL4_CPtr done_ep = atol(argv[2]);

    char *address = (char *)START_ADDR;

    for (int i = 0; i < N_RUNS + 1; i++) {
        /* record time */
        ccnt_t start, end;
        /* Making sure that the page is in the cache so that the results are not influenced by that */
        read_mapping_fault(address);

        SEL4BENCH_READ_CCNT(start);
        write_mapping_fault(address);
        SEL4BENCH_READ_CCNT(end);
        results->vm_fault_map[i] = end - start;
        address += (1 << seL4_PageBits);
    }

    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static void measure_vm_fault_map_handler_fn(int argc, char **argv) {
    seL4_CPtr ep, done_ep, reply, tcb;
    volatile ccnt_t *start;
    ccnt_t end;
    fault_results_t *results;

    parse_handler_args(argc, argv, &ep, &start, &results, &done_ep, &reply, &tcb);
    seL4_CPtr *caps = (seL4_CPtr *) atol(argv[6]);

    /* signal driver to convert us to passive and block */
    if (config_set(CONFIG_KERNEL_MCS)) {
        api_nbsend_recv(done_ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
    } else {
        /* wait for first fault */
        api_recv(ep, NULL, reply);
    }

    int err;
    for (int i = 0; i < N_RUNS; i++) {
        err = seL4_ARCH_Page_Map(caps[i], SEL4UTILS_PD_SLOT, START_ADDR + i * (1 << seL4_PageBits), seL4_AllRights,
                                seL4_ARCH_Default_VMAttributes);
        DO_REAL_REPLY_RECV(ep, seL4_MessageInfo_new(0, 0, 0, 0), reply);
    }

    err = seL4_ARCH_Page_Map(caps[N_RUNS], SEL4UTILS_PD_SLOT, START_ADDR + N_RUNS * (1 << seL4_PageBits), seL4_AllRights,
                            seL4_ARCH_Default_VMAttributes);

    if (config_set(CONFIG_KERNEL_MCS)) {
        seL4_Send(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    } else {
        api_reply(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    }

    /* tell benchmark we are done and that there are no errors */
    seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* block */
    seL4_Wait(ep, NULL);
}

void run_benchmark(void *faulter_fn, void *handler_fn, seL4_CPtr done_ep)
{
    int error = sel4utils_start_thread(&fault_handler, (sel4utils_thread_entry_fn) handler_fn,
                                       (void *) N_HANDLER_ARGS, (void *) handler_argv, true);
    ZF_LOGF_IF(error, "Failed to start handler");

    if (config_set(CONFIG_KERNEL_MCS)) {
        /* convert the fault handler to passive */
        ZF_LOGD("Waiting to convert handler to passive");
        seL4_Wait(done_ep, NULL);
        ZF_LOGD("unbound sc\n");
        error = api_sc_unbind(fault_handler.sched_context.cptr);
        ZF_LOGF_IF(error, "Failed to convert to passive");
    }

    error = sel4utils_start_thread(&faulter, (sel4utils_thread_entry_fn) faulter_fn,
                                   (void *) N_FAULTER_ARGS, (void *) faulter_argv, true);
    ZF_LOGF_IF(error, "Failed to start faulter");

    /* benchmark runs */
    benchmark_wait_children(done_ep, "faulter", 1);

    if (config_set(CONFIG_KERNEL_MCS)) {
        /* convert the fault handler to active */
        ZF_LOGD("Rebound sc\n");
        error = api_sc_bind(fault_handler.sched_context.cptr, fault_handler.tcb.cptr);
        ZF_LOGF_IF(error, "Failed to convert to active");
    }
    benchmark_wait_children(done_ep, "fault handler", 1);

    error = seL4_TCB_Suspend(faulter.tcb.cptr);
    ZF_LOGF_IF(error, "Failed to suspend faulter");
    error = seL4_TCB_Suspend(fault_handler.tcb.cptr);
    ZF_LOGF_IF(error, "Failed to suspend fault handler");
}

static void run_fault_benchmark(env_t *env, fault_results_t *results)
{
    /* allocate endpoint */
    vka_object_t fault_endpoint = {0};
    UNUSED int error = vka_alloc_endpoint(&env->slab_vka, &fault_endpoint);
    assert(error == 0);

    vka_object_t done_ep = {0};
    error = vka_alloc_endpoint(&env->slab_vka, &done_ep);
    assert(error == 0);

    seL4_CPtr pd;
    seL4_CPtr pt;
    seL4_CPtr caps[N_RUNS + 1];

#ifdef CONFIG_ARCH_AARCH64
        vka_object_t untyped;
        cspacepath_t untyped_path;

        if ((vka_alloc_untyped(&env->delegate_vka, env->args->untyped_size_bits - 5,
                               &untyped)) != 0) {
            ZF_LOGF("alloc untyped fail\n");
        }
        vka_cspace_make_path(&env->delegate_vka, untyped.cptr, &untyped_path);


        int err = vka_cspace_alloc(&env->delegate_vka, &pd);
        assert(!err);


        err = seL4_Untyped_Retype(untyped_path.capPtr, seL4_ARM_PageDirectoryObject,
                                  seL4_PageBits, SEL4UTILS_CNODE_SLOT, SEL4UTILS_CNODE_SLOT,
                                  untyped_path.capDepth, pd, 1);
        ZF_LOGE("%d", untyped_path.capDepth);
        err = seL4_ARM_PageDirectory_Map(pd, SEL4UTILS_PD_SLOT, START_ADDR, seL4_ARCH_Default_VMAttributes);
        assert(!err);

        err = vka_cspace_alloc(&env->delegate_vka, &pt);
        err = seL4_Untyped_Retype(untyped_path.capPtr, seL4_ARCH_PageTableObject,
                                  seL4_PageBits, SEL4UTILS_CNODE_SLOT, SEL4UTILS_CNODE_SLOT,
                                  untyped_path.capDepth, pt, 1);
        err = seL4_ARCH_PageTable_Map(pt, SEL4UTILS_PD_SLOT, START_ADDR, seL4_ARCH_Default_VMAttributes);
        assert(!err);

        for (int i = 0; i < N_RUNS + 1; i++) {
            err = vka_cspace_alloc(&env->delegate_vka, &caps[i]);
            assert(!err);
            err = seL4_Untyped_Retype(untyped_path.capPtr, seL4_ARCH_4KPage,
                                      seL4_PageBits, SEL4UTILS_CNODE_SLOT, SEL4UTILS_CNODE_SLOT,
                                      untyped_path.capDepth, caps[i], 1);
            int err = seL4_ARM_Page_Map(caps[i], SEL4UTILS_PD_SLOT, START_ADDR + i * (1 << seL4_PageBits), seL4_CanRead,
                                        seL4_ARCH_Default_VMAttributes);
            assert(!err);
        }
#endif

    /* create faulter */
    ccnt_t start = 0;
    benchmark_configure_thread(env, fault_endpoint.cptr, seL4_MinPrio + 1, "faulter", &faulter);
    sel4utils_create_word_args(faulter_args, faulter_argv, N_FAULTER_ARGS, (seL4_Word) &start,
                               (seL4_Word) results, done_ep.cptr);

    /* create fault handler */
    benchmark_configure_thread(env, seL4_CapNull, seL4_MinPrio, "fault handler", &fault_handler);
    sel4utils_create_word_args(handler_args, handler_argv, N_HANDLER_ARGS,
                               fault_endpoint.cptr, (seL4_Word) &start,
                               (seL4_Word) results, done_ep.cptr, fault_handler.reply.cptr, faulter.tcb.cptr, (seL4_Word) caps);

    /* Benchmark vm fault */
    run_benchmark(measure_vm_fault_fn, measure_vm_fault_handler_fn, done_ep.cptr);

    /* Benchmark vm fault */
    run_benchmark(measure_vm_fault_reply_fn, measure_vm_fault_reply_handler_fn, done_ep.cptr);

    /* benchmark fault */
    run_benchmark(measure_fault_fn, measure_fault_handler_fn, done_ep.cptr);

    /* benchmark reply */
    run_benchmark(measure_fault_reply_fn, measure_fault_reply_handler_fn, done_ep.cptr);

    /* benchmark round_trip */
    run_benchmark(measure_fault_roundtrip_fn, measure_fault_roundtrip_handler_fn, done_ep.cptr);

#ifdef CONFIG_ARCH_AARCH64
/* Benchmark vm fault mapping  */
        run_benchmark(measure_vm_fault_map_fn, measure_vm_fault_map_handler_fn, done_ep.cptr);

        for (int i = 0; i < N_RUNS + 1; i++) {
            err = seL4_ARCH_Page_Unmap(caps[i]);
            ZF_LOGF_IFERR(err, "unmap page failed\n");
        }

        err = seL4_ARM_PageTable_Unmap(pt);
        ZF_LOGF_IFERR(err, "unmap page table failed\n");

        err = seL4_ARM_PageDirectory_Unmap(pd);
        ZF_LOGF_IFERR(err, "unmsp page dir failed\n");
#endif
}

void measure_overhead(fault_results_t *results)
{
    ccnt_t start, end;
    seL4_CPtr ep = 0;
    UNUSED seL4_Word mr0 = 0;
    UNUSED seL4_CPtr reply = 0;

    /* overhead of reply recv stub + cycle count */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_REPLY_RECV_1(ep, mr0, reply);
        SEL4BENCH_READ_CCNT(end);
        results->reply_recv_1_overhead[i] = (end - start);
    }

    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        DO_NOP_REPLY_RECV(ep, seL4_MessageInfo_new(0, 0, 0, 0), reply);
        SEL4BENCH_READ_CCNT(end);
        results->reply_recv_overhead[i] = (end - start);
    }

    /* overhead of cycle count */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results->ccnt_overhead[i] = (end - start);
    }
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 2,
        [seL4_EndpointObject] = 2,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 2,
        [seL4_ReplyObject] = 2,
#endif
    };

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(fault_results_t),
              object_freq
          );
}

int main(int argc, char **argv)
{
    UNUSED int error;
    fault_results_t *results;
    results = (fault_results_t *) env->results;

    sel4bench_init();

    measure_overhead(results);
    run_fault_benchmark(env, results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}