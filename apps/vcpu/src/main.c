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

/** @file VCPU benchmark VMM.
 *
 * This file acts as a VMM for VCPU threads which it will manage and support.
 * These VCPU threads will execute and perform various operations and record the
 * cycle counts for them, and then report the results back to this VMM.
 *
 * The reason why multiple guests are supported is because eventually we want to
 * update this app to launch the guest VCPU threads in their own IPA address
 * spaces so that we can benchmark a full world switch between two (or more)
 * guests.
 *
 * Multi-guest support is already supported but the actual benchmarks for the
 * world switch between VCPU threads aren't implemented since there's currently
 * no trivial way to launch a child process within an sel4bench subapp.
 *
 * This app also inherently documents several "interesting" (?) cache and other
 * issues when IPC-ing and doing unorthodox things between a native thread and a
 * VCPU thread within the same IPA address space.
 */

#include <autoconf.h>
#include <sel4benchvcpu/gen_config.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <sel4utils/strerror.h>
#include <sel4bench/kernel_logging.h>
#include <string.h>
#include <stdarg.h>
#include <utils/util.h>
#include <utils/attribute.h>
#include <vka/vka.h>
#include <vka/object_capops.h>

#include <benchmark.h>
#include <vcpu.h>

/* Problem is that these next 2 bits' function changes depending on whether
 * or not the implementatin has secure mode.
 *
 * If secure mode exists, then these 2 bits control SECURE PL0&1 cycle
 * counting, and the other 3 bits control NON-SECURE PL0,1&2 cycle
 * counting.
 *
 * However! If secure mode is not implemented, the other 3 bits are RES0!
 * So you shouldn't write to them.
 */
#define VCPU_BENCH_ARMV8A_PMCCFILTR_SECURE_PL1_CCNT_EN           (BIT(31))
#define VCPU_BENCH_ARMV8A_PMCCFILTR_SECURE_PL0_CCNT_EN           (BIT(30))

/* If secure mode is not implemented, these 3 bits are RES0. */
#define VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN        (BIT(27))
#define VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL1_CCNT_EN        (BIT(29))
#define VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL0_CCNT_EN        (BIT(28))

#ifndef CONFIG_CYCLE_COUNT

#define GENERIC_COUNTER_MASK (BIT(0))
#undef READ_COUNTER_BEFORE
#undef READ_COUNTER_AFTER
#define READ_COUNTER_BEFORE(x) sel4bench_get_counters(GENERIC_COUNTER_MASK, &x);
#define READ_COUNTER_AFTER(x) sel4bench_get_counters(GENERIC_COUNTER_MASK, &x)
#endif

#ifndef CONFIG_DEBUG_BUILD
#define seL4_DebugCapIdentify(...) -1
#endif

/* This is the global data shared between the guests and the VMM. It's grouped
 * into a struct to make cleaning it to PoU easier.
 */
vcpu_bm_shared_globals_t sg;

/* This is used by the VMM to recv faults for all guest threads. */
vka_object_t all_guests_fault_ep_vkao;
/* This is the slot for the saved reply cap used by the VMM's main loop. */
cspacepath_t reply_cap_cspath;

// One NULL guest for ID 0.
guest_t guests[VCPU_BENCH_N_GUESTS + 1];
const char *vcpu_benchmark_names[] = {
    "VCPU_BENCHMARK_HVC_PRIV_ESCALATE",
    "VCPU_BENCHMARK_ERET_PRIV_DESCALATE",
    "VCPU_BENCHMARK_HVC_NULL_SYSCALL",
    "VCPU_BENCHMARK_CALL_SYSCALL",
    "VCPU_BENCHMARK_REPLY_SYSCALL",
};

/** Initialize one guest_t structure to represent a runnable VCPU thread.
 */
void guest_exec(guest_t *g)
{
    int error;
    seL4_UserContext regs = {0};
    va_list args;
    seL4_Word *msgregs;

    regs.pc = (seL4_Word)&guest__start;
    regs.sp = regs.x29 = (seL4_Word)&g->stack[VCPU_BENCH_GUEST_STACK_SIZE];
    regs.spsr = 0x5;

    /* ABI at guest initialization: X0 register contains a pointer to the
     * guest's metadata struct.
     */
    regs.x0 = (seL4_Word)g;
    regs.x1 = sizeof(*g);
    error = seL4_ARM_VCPU_WriteRegs(g->vcpu_vkao.cptr,
                                    seL4_VCPUReg_SP_EL1, regs.sp);
    assert(error == seL4_NoError);

    ZF_LOGD("Guest%d about to exec, pc %lx, sp %lx, guest struct @%p.\n",
            g->id, regs.pc, regs.sp, g);

    error = seL4_TCB_WriteRegisters(g->thread.tcb.cptr, 1, 0,
                                    sizeof(seL4_UserContext) / sizeof(seL4_Word),
                                    &regs);
    assert(error == seL4_NoError);
    assert(g->magic == VCPU_BENCH_SHDATA_MAGIC);
}

void guest_init(guest_t *g, env_t *env, int id)
{
    int error;
    seL4_ARM_VCPU_ReadRegs_t vrerr;
    seL4_CPtr parent_cspace_cap, parent_vspace_cap;
    char tname[16];
    seL4_Word sctlr;
    seL4_Word cspace_root_data;
    int captype;

    assert(g != NULL);

    memset(g, 0, sizeof(*g));
    g->magic = VCPU_BENCH_SHDATA_MAGIC;
    g->id = id;

    parent_vspace_cap = simple_get_pd(&env->simple);
    assert(parent_vspace_cap == SEL4UTILS_PD_SLOT);

    parent_cspace_cap = simple_get_cnode(&env->simple);
    assert(parent_cspace_cap == SEL4UTILS_CNODE_SLOT);

    /* First the fault ep end to be given to the kernel. Set a bit in the badge
     * value so the handler can quickly tell the difference between message
     * types.
     */
    error = vka_mint_object(&env->delegate_vka, &all_guests_fault_ep_vkao,
                            &g->badged_fault_ep, seL4_AllRights,
                            (VCPU_BENCH_MSG_IS_FAULT_BIT | g->id));
    assert(error == 0);
    captype = seL4_DebugCapIdentify(g->badged_fault_ep.capPtr);
    ZF_LOGD("Badged Fault ep cap is %lu, identified as %d.\n",
            g->badged_fault_ep.capPtr, captype);
    /* Next another cap to the same EP obj which the guest will use to send IPC
     * to the VMM thread.
     */
    error = vka_mint_object(&env->delegate_vka, &all_guests_fault_ep_vkao,
                            &g->badged_fault_ep_ipc_copy, seL4_AllRights, g->id);
    assert(error == 0);

    captype = seL4_DebugCapIdentify(g->badged_fault_ep_ipc_copy.capPtr);
    ZF_LOGD("Badged IPC endpoint cap is %lu, identified as %d\n",
            g->badged_fault_ep_ipc_copy.capPtr, captype);

    error = vka_alloc_vcpu(&env->delegate_vka, &g->vcpu_vkao);
    assert(error == 0);

    /** The guest will share the same page tables as us so that it will see
     * our vaddrspace as if it was its own paddrspace.
     *
     * It'll also share our cspace.
     */
    cspace_root_data = api_make_guard_skip_word(
                           seL4_WordBits - CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);

    error = sel4utils_configure_thread(&env->delegate_vka,
                                       &env->vspace, &env->vspace,
                                       g->badged_fault_ep.capPtr,
                                       parent_cspace_cap,
                                       /* We were spawned with sel4utils_configure_* */
                                       cspace_root_data,
                                       &g->thread);

    assert(error == 0);

    error = seL4_TCB_SetPriority(
                g->thread.tcb.cptr, simple_get_tcb(&env->simple), seL4_MaxPrio / 2);

    assert(error == 0);

    error = seL4_ARM_VCPU_SetTCB(g->vcpu_vkao.cptr, g->thread.tcb.cptr);
    assert(error == 0);

    snprintf(tname, 16, "Guest%d", id);
    NAME_THREAD(g->thread.tcb.cptr, tname);

    vrerr = seL4_ARM_VCPU_ReadRegs(g->vcpu_vkao.cptr, seL4_VCPUReg_SCTLR);
    assert(vrerr.error == 0);

    /* Uncomment this to disable guest I and D cache */
    //vrerr.value &= ~(BIT(12)|BIT(2));
    error = seL4_ARM_VCPU_WriteRegs(g->vcpu_vkao.cptr, seL4_VCPUReg_SCTLR, vrerr.value);
    assert(error == 0);
    vrerr = seL4_ARM_VCPU_ReadRegs(g->vcpu_vkao.cptr, seL4_VCPUReg_CPACR);
    assert(vrerr.error == 0);

    /* Disable guest FPU coprocessor trapping */
    seL4_ARM_VCPU_WriteRegs(g->vcpu_vkao.cptr, seL4_VCPUReg_CPACR,
                            (vrerr.value | (0x3 << 20) | (0x3 << 16)));
    assert(error == 0);
}

static inline uintptr_t get_pmcr_el0(void)
{
    uintptr_t ret;

    asm volatile(
        "mrs %0, pmcr_el0\n"
        : "=r"(ret));
    return ret;
}

/**
 * This thread will act as the VMM thread.
 */
__attribute__((noinline)) static void *
getPc(void)
{
    void *ret;

    asm volatile(
        "mov %0, lr\n"
        : "=r"(ret));

    return ret;
}

static void set_pmccfiltr_el0(uint32_t val)
{
    asm volatile("msr pmccfiltr_el0, %0\n"
                 :: "r"(val));
}

static uint32_t get_pmccfilter_el0(void)
{
    uint32_t val;

    asm volatile("mrs %0, pmccfiltr_el0\n"
                 :"=r"(val));
    return val;
}

/** Toggles the cycle counter on/off while executing in PL2.
 *
 * For the HYP benchmarks, we need to have the cycle counter running
 * while we are in PL2 or else we won't be able to benchmark the
 * PL2 kernel.
 *
 * It seems like the other benchmarks require PL2 cycle counting to
 * be disabled since sel4bench explicitly sets PL2 CCounting off in
 * sel4bench_init().
 *
 * This function is provided to enable the VCPU benchmarks to exist
 * side by side with the other benchmarks by toggling them on/off inside
 * of the VCPU benchmarks.
 *
 * FIXME:
 *
 * The processor currently refuses to latch the value and actually count the
 * cycles it uses while executing in PL2, so this means that the prvilege-de-
 * escalation benchmark's numbers can (should) be lower.
 *
 * @param[in] onoff Boolean stating which state to toggle PL2 cycle counting
 *                  into.
 */
static void vcpu_bm_toggle_cycle_counting_in_pl2(bool onoff)
{
    /* We can use some deductive reasoning:
     * ARMv7-A manual, Section A1.4.2 says that if virtualization extensions
     * (HYP mode) are supported, then the security extensions must *ALSO* be
     * implemented. We can therefore assume that every CPU with hyp mode also
     * has an EL3.
     *
     * Therefore, if CONFIG_ARM_HYPERVISOR_SUPPORT is set, we should assume that
     * in order to enable PL2 cycle counting, we can safely access and toggle
     * SEL4BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN.
     */
    uint32_t pmccfiltr;

    pmccfiltr = get_pmccfilter_el0();

    if (onoff) {
        pmccfiltr &= ~VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN;
        set_pmccfiltr_el0(pmccfiltr);

        /* These asserts never trigger, so the processor does set/clear the
         * bit, but it disobeys the bit.
         */
        assert((get_pmccfilter_el0() &
                VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN) == 0);
    } else {
        pmccfiltr |= VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN;
        set_pmccfiltr_el0(pmccfiltr);

        assert(get_pmccfilter_el0() &
               VCPU_BENCH_ARMV8A_PMCCFILTR_NONSECURE_PL2_CCNT_EN);
    }
}

static seL4_MessageInfo_t vmm_handle_hypcall(env_t *env, seL4_Word badge, seL4_MessageInfo_t tag)
{
    int ret = 0;
    assert(badge != 0);
    seL4_Word label;

    label = seL4_MessageInfo_get_label(tag);

    switch (label) {
    case HYPCALL_SYS_PUTC: {
        char c;

        /* No IPC buffer invalidate needed because MR(0-3) are registers and not
         * actually stored in the IPC buffer. See comment in REPORT_END_RESULTS.
         */
        THREAD_MEMORY_ACQUIRE();

        c = seL4_GetMR(0);
        putc(c, stdout);
        break;
    }

    case HYPCALL_SYS_PUTS: {
        int strlen;

        /* No IPC buffer invalidate needed because MR(0-3) are registers and not
         * actually stored in the IPC buffer. See comment in REPORT_END_RESULTS.
         */
        THREAD_MEMORY_ACQUIRE();

        strlen = seL4_GetMR(0);
        /* If you want to see the printf hypercall messages that the guest
         * makes to the VMM, change this to a `printf()` or update the
         * ZF_LOG_LEVEL to make the following output visible.
         */
        ZF_LOGI("Guest%lu: puts(): %s", badge, guests[badge].printf_buffer);
        break;
    }

    case HYPCALL_SYS_EXIT_THREAD: {
        int exitstatus;
        bool was_el1_fault;
        seL4_Word miscdata;

        /* No IPC buffer invalidate needed because MR(0-3) are registers and not
         * actually stored in the IPC buffer. See comment in REPORT_END_RESULTS.
         */
        THREAD_MEMORY_ACQUIRE();

        assert(seL4_MsgFastRegisters >= 4);
        was_el1_fault = seL4_GetMR(0);
        exitstatus = seL4_GetMR(1);
        miscdata = seL4_GetMR(2);

        ZF_LOGI("Guest%lu exited with status %d (EL1 Fault? %u). Misc %lx\n",
                badge, exitstatus, was_el1_fault, miscdata);

        guests[badge].exitstatus = exitstatus;
        /* Kill the thread. */
        sel4utils_clean_up_thread(&env->delegate_vka, &env->vspace,
                                  &guests[badge].thread);
        ret = 0;
        break;
    }

    case HYPCALL_SYS_GET_SEL4_REPLY_START_STAMP:
    case HYPCALL_SYS_GET_SEL4_CALL_END_STAMP: {
        uint64_t stamp;

        SEL4BENCH_READ_CCNT(stamp);

        /* No IPC buffer invalidate needed because MR(0-3) are registers and not
         * actually stored in the IPC buffer. See comment in REPORT_END_RESULTS.
         */

        seL4_SetMR(0, stamp);
        if (label == HYPCALL_SYS_GET_SEL4_CALL_END_STAMP) {
            THREAD_MEMORY_RELEASE();
        }
        ret = 1;
        break;
    }

    case HYPCALL_SYS_BM_REPORT_END_RESULTS: {
        enum vcpu_benchmarks bm;
        uint64_t min, max,
                 clipped_avg, complete_avg, clipped_total, complete_total,
                 n_anomalies;

        /* Explanation:
         *
         * Invalidate the IPC buffer from DCache. When switching from a VCPU
         * thread to a native thread, changes made to the IPC buffer in the
         * VCPU thread may not have been committed coherently to the non-HYP
         * world's cache subsystem.
         *
         * The result figures from the guest can be shared back to the VMM
         * thread using shared memory, and the CPtrs to the underlying frames
         * can be obtained using `vspace_get_cap()` -- using this function
         * this app can be reworked to use seL4_ARM_Page_Invalidate/Clean_Data()
         * to manage the cache incoherencies.
         */
        seL4_ARM_Page_Invalidate_Data(guests[badge].thread.ipc_buffer,
                                      0, BIT(seL4_PageBits) - 1);
        THREAD_MEMORY_ACQUIRE();

        assert(seL4_MsgFastRegisters >= 4);
        bm = seL4_GetMR(0);
        assert(bm < VCPU_BENCHMARK_N_BENCHMARKS);
        min = seL4_GetMR(1);
        max = seL4_GetMR(2);
        clipped_avg = seL4_GetMR(3);
        complete_avg = seL4_GetMR(4);
        clipped_total = seL4_GetMR(5);
        complete_total = seL4_GetMR(6);
        n_anomalies = seL4_GetMR(7);

        /* The guest calls this function to report the benchmark results as it
         * obtains them, so if you change this to `printf()` or change the
         * ZF_LOG_LEVEL you can see the results as they are reported by the
         * guest.
         */
        ZF_LOGI("Results: BM '%s': min %lu, max %lu, "
                "clip avg %lu, comp avg %lu, "
                "clip total %lu, comp tot %lu (%lu anomalies).\n",
                vcpu_benchmark_names[bm],
                min, max, clipped_avg, complete_avg,
                clipped_total, complete_total,
                n_anomalies);

        break;
    }

    case HYPCALL_SYS_BM_REPORT_DEEP_RESULTS: {
        enum vcpu_benchmarks bm;
        uint64_t start, end;
        int idx;
        vcpu_benchmark_overall_results_t *results;

        /* No IPC buffer invalidate needed because MR(0-3) are registers and not
         * actually stored in the IPC buffer. See comment in REPORT_END_RESULTS.
         */
        THREAD_MEMORY_ACQUIRE();

        assert(seL4_MsgFastRegisters >= 4);
        bm = seL4_GetMR(0);
        assert(bm < VCPU_BENCHMARK_N_BENCHMARKS);
        idx = seL4_GetMR(1);
        start = seL4_GetMR(2);
        end = seL4_GetMR(3);

        /* Store the reported results into the shared mem to be post-processed
         */
        results = (vcpu_benchmark_overall_results_t *)env->results;

        results->results[bm].deep_data.start[idx] = start;
        results->results[bm].deep_data.end[idx] = end;
        results->results[bm].deep_data.elapsed[idx] = end - start;

        /* The guest calls this function to report the benchmark results as it
         * obtains them, so if you change this to `printf()` or change the
         * ZF_LOG_LEVEL you can see the results as they are reported by the
         * guest.
         */
        ZF_LOGI("Results: BM '%s': iter %i, start %lu, end %lu\n",
                vcpu_benchmark_names[bm], idx,
                start, end);

        break;
    }

    default:
        ZF_LOGE("Unknown Hypcall from guest %lu.\n", badge);
        break;
    }

    return seL4_MessageInfo_set_length(tag, ret);
}

static seL4_MessageInfo_t vmm_handle_sel4_fault(env_t *env, seL4_Word badge, seL4_MessageInfo_t tag)
{
    seL4_Word guestid, fault_type;

    fault_type = seL4_MessageInfo_get_label(tag);
    guestid = badge & ~(VCPU_BENCH_MSG_IS_FAULT_BIT);

    ZF_LOGW("seL4 fault %lu (%s) in thread for guest %lu.\n",
            fault_type, sel4_strfault(fault_type), guestid);

    switch (fault_type) {
    case seL4_Fault_VMFault:
        ZF_LOGW("VMFault: IP %lx, Instr? %lu. (If Data, DFAR: 0x%lx).\n"
                "ESR %lx.\n",
                seL4_GetMR(seL4_VMFault_IP),
                seL4_GetMR(seL4_VMFault_PrefetchFault),
                seL4_GetMR(seL4_VMFault_Addr),
                seL4_GetMR(seL4_VMFault_FSR));
        break;
    }

    return seL4_MessageInfo_new(1, 0, 0, 0);
}

static int vcpu_bench_setup_log_buffer_frame(env_t *env)
{
    int err;

    err = vka_alloc_frame(&env->delegate_vka, seL4_LargePageBits,
                          &sg.log_buffer_frame_vkao);

    assert(err == 0);

    err = kernel_logging_set_log_buffer(sg.log_buffer_frame_vkao.cptr);
    if (err != 0) {
        ZF_LOGE("Failed to set log buffer frame using cap %lu.\n",
                sg.log_buffer_frame_vkao.cptr);

        return err;
    }

    sg.sel4_log_buffer = vspace_map_pages(&env->vspace,
                                          &sg.log_buffer_frame_vkao.cptr, NULL,
                                          seL4_AllRights, 1, seL4_LargePageBits,
                                          1);
    if (sg.sel4_log_buffer == NULL) {
        ZF_LOGE("Failed to map log buffer into VMM addrspace.\n");
        return -1;
    }

    memset((void *)sg.sel4_log_buffer, 0, BIT(seL4_LargePageBits));
    ZF_LOGD("Log buffer (cptr %lu) mapped to vaddr %p.\n",
            sg.log_buffer_frame_vkao.cptr, sg.sel4_log_buffer);
    return 0;
}

static ccnt_t get_ccnt_read_overhead_avg(void)
{
    ccnt_t prev_stamp, min_overhead, max_overhead, total_overhead, avg_overhead;

    /* This is separate from the N_ITERATIONS defined for the main benchmark
     * loops.
     */
    const int N_ITERATIONS = 10;

    /* Guarantees that the first use of MIN() will find this too large */
    min_overhead = 100000;
    /* Guarantees that the first use of MAX() will find this too small */
    max_overhead = 0;
    total_overhead = 0;

    SEL4BENCH_READ_CCNT(prev_stamp);

    for (int i = 0; i < N_ITERATIONS; i++) {
        ccnt_t curr_stamp, curr_overhead;

        SEL4BENCH_READ_CCNT(curr_stamp);
        curr_overhead = curr_stamp - prev_stamp;


        total_overhead += curr_overhead;

        min_overhead = MIN(min_overhead, curr_overhead);
        max_overhead = MAX(max_overhead, curr_overhead);

        prev_stamp = curr_stamp;
    }

    avg_overhead = total_overhead / N_ITERATIONS;

    ZF_LOGI("CCNT read op overheads: "
            "min %lu, max %lu, avg %lu.\n",
            min_overhead, max_overhead, avg_overhead);

    return avg_overhead;
}

int main(int argc, char **argv)
{
    int err, n_guests_exited;
    env_t *env;
    cspacepath_t ep_path, result_ep_path;
    vcpu_benchmark_overall_results_t *overall_results;

    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 4,
        [seL4_EndpointObject] = 2,
#ifdef CONFIG_KERNEL_MCS
        [seL4_SchedContextObject] = 4,
        [seL4_ReplyObject] = 4
#endif
    };

    env = benchmark_get_env(argc, argv,
                            sizeof(vcpu_benchmark_overall_results_t),
                            object_freq);
    overall_results = env->results;

    sel4bench_init();

    err = vka_alloc_endpoint(&env->delegate_vka, &all_guests_fault_ep_vkao);
    assert(err == 0);

    if (VCPU_BENCH_N_GUESTS > VCPU_BENCH_MAX_N_GUESTS) {
        ZF_LOGE("Chosen number of guests (%lu) exceeds max (%lu)! Aborting.\n",
                VCPU_BENCH_MAX_N_GUESTS, VCPU_BENCH_MAX_N_GUESTS);
        return -1;
    }

    /* Measure CCNT read operation overhead */
    overall_results->ccnt_read_overhead = get_ccnt_read_overhead_avg();

    ZF_LOGD("About to setup PL2 cycle counters.\n");
    vcpu_bm_toggle_cycle_counting_in_pl2(true);

    ZF_LOGD("About to setup log buffer\n");
    err = vcpu_bench_setup_log_buffer_frame(env);
    if (err != 0) {
        ZF_LOGE("Failed to setup log buffer for timestamps. Err %d.\n", err);
        return err;
    }

    ZF_LOGD("About to guest init %d guests: current PC vaddr is %p.\n",
            VCPU_BENCH_N_GUESTS, getPc());
    for (int i = 1; i <= VCPU_BENCH_N_GUESTS; i++) {
        guest_init(&guests[i], env, i);
    }

    for (int i = 1; i <= VCPU_BENCH_N_GUESTS; i++) {
        ZF_LOGD("About to exec guest %d\n", i);
        guest_exec(&guests[i]);
    }

    err = vka_cspace_alloc_path(&env->delegate_vka, &reply_cap_cspath);
    if (err != 0) {
        ZF_LOGE("Failed to alloc slot for reply cap.\n");
        return -1;
    }

    n_guests_exited = 0;
    for (;;) {
        seL4_MessageInfo_t tag, replytag;
        seL4_Word badge;
        bool isSel4Fault = 0;
        int nMsgRegs = 0;

        tag = seL4_Recv(all_guests_fault_ep_vkao.cptr, &badge);
        err = seL4_CNode_SaveCaller(simple_get_cnode(&env->simple),
                                    reply_cap_cspath.capPtr,
                                    reply_cap_cspath.capDepth);

        if (err != seL4_NoError) {
            ZF_LOGE("Failed to save reply cap for some reason! Err %d\n", err);
            return -1;
        }

        if (badge & VCPU_BENCH_MSG_IS_FAULT_BIT) {
            isSel4Fault = 1;

            replytag = vmm_handle_sel4_fault(env, badge, tag);
        } else {
            replytag = vmm_handle_hypcall(env, badge, tag);

            if (seL4_MessageInfo_get_label(tag) == HYPCALL_SYS_EXIT_THREAD) {
                /* Clear the reply cap slot for the next loop iteration. */
                seL4_CNode_Delete(simple_get_cnode(&env->simple),
                                  reply_cap_cspath.capPtr,
                                  reply_cap_cspath.capDepth);

                n_guests_exited++;
                if (n_guests_exited == VCPU_BENCH_N_GUESTS) {
                    break;
                } else {
                    /* If the thread has been killed, don't reply to it. */
                    continue;
                }
            }
        }

        seL4_Send(reply_cap_cspath.capPtr, replytag);
    }

    /* Disable PL2 cycle counting again so we can co-exist with the other BMs */
    vcpu_bm_toggle_cycle_counting_in_pl2(false);

    /* We've exited the loop, and potentially written our benchmark result data
     * to the shared mem buffer. Commit any data in the write buffer to cache.
     */
    THREAD_MEMORY_RELEASE();

    /* Check the return value from each guest thread. If any failed, then the
     * benchmark results are in an unknown state, so return error.
     */
    for (int i = 1; i <= VCPU_BENCH_N_GUESTS; i++) {
        if (guests[i].exitstatus != 0) {
            ZF_LOGE("VCPUBench: Returning finished status %d.\n",
                    guests[i].exitstatus);

            /* benchmark_finished() doesn't return */
            benchmark_finished(guests[i].exitstatus);
        }
    }

    ZF_LOGI("VCPUBench: Exiting successfully.\n");
    benchmark_finished(0);
    return 0;
}
