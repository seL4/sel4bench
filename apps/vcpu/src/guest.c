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

#include <stdint.h>
#include <utils/fence.h>
#include <sel4bench/kernel_logging.h>
#include <vcpu.h>

/* Between the seL4_HypBenchmarkResetLog() and the kcall_invoke_null_kcall()
 * that follows it, the log buffer is advanced once by the
 * seL4_HypBenchmarkResetLog() syscall itself, and then the timestamps for the
 * actual kcall_invoke_null_kcall() are written to index 1.
 */
#define GUEST_BM_LOG_BUFFER_INDEX    (1)

static int _sel4_dbg_puts(const char *str);
static int _sel4_dbg_printf(guest_t *g, const char *fmt, ...);

#ifdef CONFIG_PRINTING
#define sel4_dbg_puts(str) _sel4_dbg_puts((str))
#define sel4_dbg_printf(g, fmt, ...) _sel4_dbg_printf((g), (fmt), ## __VA_ARGS__)
#else
#define seL4_HypDebugPutChar(...)
#define sel4_dbg_puts(str)
#define sel4_dbg_printf(g, fmt, ...)
#endif

#ifndef CONFIG_DEBUG_BUILD
#define seL4_HypDebugCapIdentify(...) (-1)
#endif

extern void **aarch64_el1_vector_table;
typedef void (guest_bm_stamp_getter_fn)(guest_t *self,
                                        vcpu_benchmark_results_t *r,
                                        uint64_t *start, uint64_t *end);

static inline void
cleanDCacheByVa(seL4_Word va, bool PoC)
{
    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);

    if (PoC) {
        asm volatile("dc cvac, %0\n" : : "r" (va));
    } else {
        asm volatile("dc cvau, %0\n" : : "r" (va));
    }
}

static inline void
invalidateDCacheByVa(seL4_Word va)
{
    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);

    /* DC IVA always goes to PoC. */
    asm volatile("dc ivac, %0\n" : : "r" (va));
}

static inline void
invalidateICacheByVa(seL4_Word va)
{
    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);
    asm volatile("ic ivau, %0\n" : : "r" (va));
}

static void
cleanGuestContextToDCache(guest_t *self, bool PoC)
{
    uintptr_t va = (uintptr_t)self;

    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);
    for (; va < (uintptr_t)&self[1]; va += VCPU_BENCH_CACHE_LINE_SZ) {
        cleanDCacheByVa(va, PoC);
    }
}

static void
cleanSharedGlobalsToDCache(bool PoC)
{
    uintptr_t va = (uintptr_t)&sg;

    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);
    for (; va < (uintptr_t) & (&sg)[1]; va += VCPU_BENCH_CACHE_LINE_SZ) {
        cleanDCacheByVa(va, PoC);
    }
}

static void
invalidateLogBufferFromDCache(void)
{
    uintptr_t va = (uintptr_t)sg.sel4_log_buffer;

    va &= ~(VCPU_BENCH_CACHE_LINE_SZ - 1);
    for (; va < (uintptr_t)sg.sel4_log_buffer + BIT(seL4_PageBits);
            va += VCPU_BENCH_CACHE_LINE_SZ) {
        invalidateDCacheByVa(va);
    }
}

static inline void
invalidateTlbAll(void)
{
    asm volatile("tlbi alle1\n\t");
}

static inline void
arm_hyp_sys_null(seL4_Word sys)
{
    register seL4_Word scno asm("x7") = sys;
    asm volatile (
        "hvc #0"
        : /* no outputs */
        : "r"(scno)
    );
}

static inline void
arm_hyp_sys_send_recv(seL4_Word sys, seL4_Word dest,
                      seL4_Word *out_badge,
                      seL4_Word info_arg, seL4_Word *out_info,
                      seL4_Word *in_out_mr0, seL4_Word *in_out_mr1,
                      seL4_Word *in_out_mr2, seL4_Word *in_out_mr3)
{
    register seL4_Word destptr asm("x0") = dest;
    register seL4_Word info asm("x1") = info_arg;

    /* Load beginning of the message into registers. */
    register seL4_Word msg0 asm("x2") = *in_out_mr0;
    register seL4_Word msg1 asm("x3") = *in_out_mr1;
    register seL4_Word msg2 asm("x4") = *in_out_mr2;
    register seL4_Word msg3 asm("x5") = *in_out_mr3;

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile (
        "hvc #0"
        : "+r" (msg0), "+r" (msg1), "+r" (msg2), "+r" (msg3),
        "+r" (info), "+r" (destptr)
        : "r"(scno)
        : "memory"
    );
    *out_info = info;
    *out_badge = destptr;
    *in_out_mr0 = msg0;
    *in_out_mr1 = msg1;
    *in_out_mr2 = msg2;
    *in_out_mr3 = msg3;
}

LIBSEL4_INLINE_FUNC seL4_MessageInfo_t
seL4_HypCall(seL4_CPtr dest, seL4_MessageInfo_t msgInfo)
{

    seL4_MessageInfo_t info;
    seL4_Word msg0 = seL4_GetMR(0);
    seL4_Word msg1 = seL4_GetMR(1);
    seL4_Word msg2 = seL4_GetMR(2);
    seL4_Word msg3 = seL4_GetMR(3);

    arm_hyp_sys_send_recv(seL4_SysCall, dest, &dest,
                          msgInfo.words[0], &info.words[0],
                          &msg0, &msg1, &msg2, &msg3);

    /* Write out the data back to memory. */
    seL4_SetMR(0, msg0);
    seL4_SetMR(1, msg1);
    seL4_SetMR(2, msg2);
    seL4_SetMR(3, msg3);

    return info;
}

LIBSEL4_INLINE seL4_Error
seL4_ARMHyp_Page_Invalidate_Data(seL4_ARM_Page _service, seL4_Word start_offset, seL4_Word end_offset)
{
        seL4_Error result;
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(ARMPageInvalidate_Data, 0, 0, 2);
        seL4_MessageInfo_t output_tag;

        /* Marshal and initialise parameters. */
        seL4_SetMR(0, start_offset);
        seL4_SetMR(1, end_offset);

        /* Perform the call, passing in-register arguments directly. */
        output_tag = seL4_HypCall(_service, tag);
        result = (seL4_Error) seL4_MessageInfo_get_label(output_tag);

        return result;
}

LIBSEL4_INLINE seL4_Error
seL4_ARMHyp_Page_Clean_Data(seL4_ARM_Page _service, seL4_Word start_offset, seL4_Word end_offset)
{
        seL4_Error result;
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(ARMPageClean_Data, 0, 0, 2);
        seL4_MessageInfo_t output_tag;

        /* Marshal and initialise parameters. */
        seL4_SetMR(0, start_offset);
        seL4_SetMR(1, end_offset);

        /* Perform the call, passing in-register arguments directly. */
        output_tag = seL4_HypCall(_service, tag);
        result = (seL4_Error) seL4_MessageInfo_get_label(output_tag);

        return result;
}

LIBSEL4_INLINE_FUNC void
seL4_HypYield(void)
{
    arm_hyp_sys_null(seL4_SysYield);
    asm volatile("" ::: "memory");
}

#ifdef CONFIG_PRINTING
LIBSEL4_INLINE_FUNC void
seL4_HypDebugPutChar(char c)
{
    seL4_Word unused0 = 0;
    seL4_Word unused1 = 0;
    seL4_Word unused2 = 0;
    seL4_Word unused3 = 0;
    seL4_Word unused4 = 0;
    seL4_Word unused5 = 0;

    arm_hyp_sys_send_recv(seL4_SysDebugPutChar, c, &unused0, 0, &unused1, &unused2, &unused3, &unused4, &unused5);
}

static int
_sel4_dbg_puts(const char *str)
{
    int len;

    for (len = 0; *str; str++, len++) {
        seL4_HypDebugPutChar(*str);
    }

    return len;
}

static int
_sel4_dbg_printf(guest_t *g, const char *fmt, ...)
{
    int len;
    va_list args;

    assert(g != NULL);
    assert(fmt != NULL);

    va_start(args, fmt);
    len = vsnprintf(g->printf_buffer, VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len < 0) {
        return len;
    }

    return _sel4_dbg_puts(g->printf_buffer);
}
#endif

#ifdef CONFIG_DEBUG_BUILD
LIBSEL4_INLINE_FUNC seL4_Uint32
seL4_HypDebugCapIdentify(seL4_CPtr cap)
{
    seL4_Word unused0 = 0;
    seL4_Word unused1 = 0;
    seL4_Word unused2 = 0;
    seL4_Word unused3 = 0;
    seL4_Word unused4 = 0;

    arm_hyp_sys_send_recv(seL4_SysDebugCapIdentify, cap, &cap, 0, &unused0, &unused1, &unused2, &unused3, &unused4);
    return (seL4_Uint32)cap;
}
#endif

#ifdef CONFIG_ENABLE_BENCHMARKS
/* set the log index back to 0 */
LIBSEL4_INLINE_FUNC seL4_Error
seL4_HypBenchmarkResetLog(void)
{
    seL4_Word unused0 = 0;
    seL4_Word unused1 = 0;
    seL4_Word unused2 = 0;
    seL4_Word unused3 = 0;
    seL4_Word unused4 = 0;

    seL4_Word ret;
    arm_hyp_sys_send_recv(seL4_SysBenchmarkResetLog, 0, &ret, 0,
                          &unused0, &unused1, &unused2, &unused3, &unused4);

    return (seL4_Error) ret;
}

LIBSEL4_INLINE_FUNC seL4_Word
seL4_HypBenchmarkFinalizeLog(void)
{
    seL4_Word unused0 = 0;
    seL4_Word unused1 = 0;
    seL4_Word unused2 = 0;
    seL4_Word unused3 = 0;
    seL4_Word unused4 = 0;
    seL4_Word index_ret;
    arm_hyp_sys_send_recv(seL4_SysBenchmarkFinalizeLog, 0, &index_ret, 0,
                          &unused0, &unused1, &unused2, &unused3, &unused4);


    return (seL4_Word)index_ret;
}
#endif

/** Triggers an IPC to the VMM thread.
 */
NO_INLINE static void
vmm_call(guest_t *g, int hypcallno, int argc, ...)
{
    va_list args;

    assert(g != NULL);


    switch (hypcallno) {
    case HYPCALL_SYS_PUTC:
    case HYPCALL_SYS_PUTS:
    case HYPCALL_SYS_EXIT_THREAD:
    case HYPCALL_SYS_EL1_FAULT:
    case HYPCALL_SYS_BM_REPORT_DEEP_RESULTS:
    case HYPCALL_SYS_BM_REPORT_END_RESULTS:
        va_start(args, argc);
        for (int i = 0; i < argc; i++) {
            seL4_SetMR(i, va_arg(args, seL4_Word));
        }
        va_end(args);
        THREAD_MEMORY_RELEASE();
        break;
    default:
        break;
    }

    seL4_HypCall(g->badged_fault_ep_ipc_copy.capPtr,
                 seL4_MessageInfo_new(hypcallno, 0, 0, argc));

    switch (hypcallno) {
    case HYPCALL_SYS_GET_SEL4_CALL_END_STAMP:
    case HYPCALL_SYS_GET_SEL4_REPLY_START_STAMP: {
        uint64_t *out_stamp;

        va_start(args, argc);
        out_stamp = va_arg(args, uint64_t *);
        va_end(args);
        *out_stamp = seL4_GetMR(0);
        break;
    }
    default:
        break;
    }
}

static void
vmm_call_putc(guest_t *g, char c)
{
    assert(g != NULL);

    vmm_call(g, HYPCALL_SYS_PUTC, 1, c);
}

static int
vmm_call_puts(guest_t *g, char *str)
{
    int len;
    assert(g != NULL);

    len = strnlen(str, VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE);
    if (len == VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE) {
        len--;
    }
    strncpy(g->printf_buffer, str, VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE);

    cleanGuestContextToDCache(g, 0);
    THREAD_MEMORY_RELEASE();
    vmm_call(g, HYPCALL_SYS_PUTS, 1, len);
    return len;
}

NORETURN static void
vmm_call_exit_thread(guest_t *g,
                     bool was_el1_fault, int exitstatus, seL4_Word misc)
{
    cleanGuestContextToDCache(g, 0);
    vmm_call(g, HYPCALL_SYS_EXIT_THREAD, 3, was_el1_fault, exitstatus, misc);
    __builtin_unreachable();
}

static void
vmm_call_get_sel4reply_start_stamp(guest_t *g, uint64_t *ret)
{
    vmm_call(g, HYPCALL_SYS_GET_SEL4_REPLY_START_STAMP, 1, ret);
}

static void
vmm_call_get_sel4call_end_stamp(guest_t *g, uint64_t *ret)
{
    vmm_call(g, HYPCALL_SYS_GET_SEL4_CALL_END_STAMP, 1, ret);
}

static void
vmm_call_report_end_results(guest_t *g, enum vcpu_benchmarks bm,
                            uint64_t min, uint64_t max,
                            uint64_t clipped_avg, uint64_t complete_avg,
                            uint64_t clipped_total, uint64_t complete_total,
                            uint64_t n_anomalies)
{
    vmm_call(g, HYPCALL_SYS_BM_REPORT_END_RESULTS, 8, bm,
             min, max,
             clipped_avg, complete_avg,
             clipped_total, complete_total,
             n_anomalies);
}

#if VCPU_BENCH_COLLECT_DEEP_DATA != 0
static void
vmm_call_report_deep_results(guest_t *g, enum vcpu_benchmarks bm,
                             int iteration, uint64_t min, uint64_t max)
{
    vmm_call(g, HYPCALL_SYS_BM_REPORT_DEEP_RESULTS, 4, bm,
             iteration, min, max);
}
#endif

static void
kcall_get_hvc_end_stamp(uint64_t *end)
{
    (void)end;
}

static void
kcall_get_eret_start_stamp(uint64_t *start)
{
    (void)start;
}

static void
kcall_invoke_null_kcall(int64_t sel4_syscall_num)
{
    asm volatile (
        "mov x7, %0\n"
        "hvc #0\n"
        :: "r" (sel4_syscall_num)
        : "x7");
}

NO_INLINE static int
vmm_printf(guest_t *g, char *fmt, ...)
{
    int ret;
    va_list args;

    va_start(args, fmt);
    ret = vsnprintf(g->printf_buffer, VCPU_BENCH_GUEST_PRINTF_BUFFER_SIZE,
                    fmt, args);
    va_end(args);

    if (ret < 0) {
        return ret;
    }

    vmm_call(g, HYPCALL_SYS_PUTS, 1, ret);
    return ret;
}

static inline seL4_Word
get_elr_el1(void)
{
    seL4_Word ret;

    asm volatile("mrs %0, elr_el1\n"
                 : "=r" (ret));

    return ret;
}

#define get_int_vector()    get_esr_el1()
static inline seL4_Word
get_esr_el1(void)
{
    seL4_Word ret;

    asm volatile("mrs %0, esr_el1\n"
                 : "=r" (ret));

    return ret;
}

void
guest_vector_common(int vectornum)
{
    /* Ideally we should have a way to tell the exception handler which guest it
     * is being invoked by, but right now it doesn't know that.
     */
    seL4_Word esr_el1, esr_ec, esr_iss;
    seL4_Word vector;

    vector = get_int_vector();
    esr_el1 = get_esr_el1();

    esr_ec = (esr_el1 >> 26) & 0x3F;
    esr_iss = (esr_el1 & 0x1FFFFFF);

    vmm_printf(&guests[1], "One of the guest kernels has encountered a kernel "
               "exception, entered on vector %d", vectornum);
    /* FIXME: To fully support multiple guests this needs to be changed. */
    vmm_call_exit_thread(&guests[1], true, -1, esr_el1);
}

static inline seL4_Word
get_currentel(void)
{
    seL4_Word pstate;

    asm volatile(
        "mrs %0, currentEL\n"
        : "=r" (pstate));

    pstate >>= 2;
    pstate &= 0x3;

    return pstate;
}

static inline bool
is_el1(seL4_Word pstate)
{
#ifdef CONFIG_ARCH_AARCH64
    return pstate == 0x1;
#elif defined(CONFIG_ARCH_AARCH32)
    /* 0x6 = Monitor, 0xA = Hyp. */
    return pstate > 0 && pstate != 0x6 && pstate != 0xA;
#else
#error "Unknown target arch for guest VM."
#endif
}

static inline void
guest_install_vector_table(void)
{
    asm volatile(
        "msr vbar_el1, %0\n"
        :: "r" (aarch64_el1_vector_table));
}

static inline seL4_Word
get_vbar_el1(void)
{
    seL4_Word ret;
    asm volatile("mrs %0, vbar_el1\n":"=r" (ret));
    return ret;
}

static uint64_t
guest_bm_calc_clipped_average(guest_t *self, vcpu_benchmark_results_t *r)
{
    /* We need to also take stock of the number of anomalies when averaging
     * since we don't add anomalies to the total cycle count.
     */
    return r->clipped_total / (VCPU_BENCH_N_ITERATIONS - r->n_anomalies);
}

static uint64_t
guest_bm_calc_complete_average(guest_t *self, vcpu_benchmark_results_t *r)
{
    /* We need to also take stock of the number of anomalies when averaging
     * since we don't add anomalies to the total cycle count.
     */
    return r->complete_total / VCPU_BENCH_N_ITERATIONS;
}

/** Detect potential interruptions of the current PE's operation:
 *
 * According to ARMv8 manual, D5.1.1:
 * "The Performance Monitors cycle counter, accessed through PMCCNTR_EL0 or
 * PMCCNTR, increments from the hardware processor clock, not PE clock cycles."
 * "This means that, in an implementation where PEs are multithreaded, the
 * counter continues to increment across all PEs, rather than only counting
 * cycles for which the current PE is active."
 *
 * This function factors that in by looking for unusually high differences in
 * measured time between runs.
 */
static bool
guest_bm_cycle_counter_anomaly_detected(guest_t *self,
                                        int curr_iteration,
                                        uint64_t curr_measurement_elapsed_time,
                                        vcpu_benchmark_results_t *r)
{
#if VCPU_BENCH_COLLECT_DEEP_DATA != 0
    uint64_t prev_measurement_elapsed_time, measurement_diff;

    prev_measurement_elapsed_time =
        r->deep_data.end[curr_iteration - 1] - r->deep_data.start[curr_iteration - 1];

    if (curr_measurement_elapsed_time > prev_measurement_elapsed_time) {
        measurement_diff = curr_measurement_elapsed_time
                           - prev_measurement_elapsed_time;
    } else {
        /* We don't take stock of anomalies measured by an unusually small
         * measurement because (1) they don't match what we would expect from a
         * drift, and (2) we prefer to keep unusually low numbers in because
         * they represent the best case.
         */
        measurement_diff = 0;
    }

    if (measurement_diff >= VCPU_BENCH_CYCLE_COUNTER_ANOMALY_THRESHOLD) {
        return true;
    }
#endif
    return false;
}

static void
guest_run_bm_hvc_priv_escalate(guest_t *self,
                               vcpu_benchmark_results_t *r,
                               uint64_t *start, uint64_t *end)
{
    benchmark_track_kernel_entry_t stampdata;

    strncpy(r->name, "HVC Priv Escalation", VCPU_BENCH_NAME_SIZE);

    seL4_HypYield();
    seL4_HypBenchmarkResetLog();
    SEL4BENCH_READ_CCNT(*start);
    kcall_invoke_null_kcall(seL4_SysBenchmarkNullSyscall);

    /* The kernel will be writing to this frame from a different vaddr.
     * We will be reading it back from our local mapping of it. Cache is VIPT.
     * Need to ask the kernel to clean its view of it.
     */
    seL4_ARMHyp_Page_Clean_Data(sg.log_buffer_frame_vkao.cptr,
                                0, BIT(seL4_LargePageBits) - 1);

    /* Now we need to invalidate *our* view of it. */
    invalidateLogBufferFromDCache();
    THREAD_MEMORY_ACQUIRE();
    stampdata = sg.sel4_log_buffer[GUEST_BM_LOG_BUFFER_INDEX];

    *end = stampdata.start_time;
}

static void
guest_run_bm_eret_priv_deescalate(guest_t *self,
                                  vcpu_benchmark_results_t *r,
                                  uint64_t *start, uint64_t *end)
{
    benchmark_track_kernel_entry_t stampdata;

    strncpy(r->name, "HVC Priv DE-Escalation", VCPU_BENCH_NAME_SIZE);

    seL4_HypYield();
    seL4_HypBenchmarkResetLog();
    kcall_invoke_null_kcall(seL4_SysBenchmarkNullSyscall);
    SEL4BENCH_READ_CCNT(*end);

    /* The kernel will be writing to this frame from a different vaddr.
     * We will be reading it back from our local mapping of it. Cache is VIPT.
     * Need to ask the kernel to clean its view of it.
     */
    seL4_ARMHyp_Page_Clean_Data(sg.log_buffer_frame_vkao.cptr,
                                0, BIT(seL4_LargePageBits) - 1);

    /* Now we need to invalidate *our* view of it. */
    invalidateLogBufferFromDCache();
    THREAD_MEMORY_ACQUIRE();
    stampdata = sg.sel4_log_buffer[GUEST_BM_LOG_BUFFER_INDEX];

    *start = stampdata.start_time + stampdata.duration;
}

static void
guest_run_bm_hvc_null_syscall(guest_t *self,
                              vcpu_benchmark_results_t *r,
                              uint64_t *start, uint64_t *end)
{
    strncpy(r->name, "seL4 NULL syscall", VCPU_BENCH_NAME_SIZE);
    SEL4BENCH_READ_CCNT(*start);
    kcall_invoke_null_kcall(seL4_SysBenchmarkNullSyscall);
    SEL4BENCH_READ_CCNT(*end);
}

static void
guest_run_bm_call_syscall(guest_t *self,
                          vcpu_benchmark_results_t *r,
                          uint64_t *start, uint64_t *end)
{
    strncpy(r->name, "seL4_Call full invocation", VCPU_BENCH_NAME_SIZE);
    SEL4BENCH_READ_CCNT(*start);
    vmm_call_get_sel4call_end_stamp(self, end);
}

static void
guest_run_bm_reply_syscall(guest_t *self,
                           vcpu_benchmark_results_t *r,
                           uint64_t *start, uint64_t *end)
{
    strncpy(r->name, "seL4_Reply full invocation", VCPU_BENCH_NAME_SIZE);
    vmm_call_get_sel4reply_start_stamp(self, start);
    SEL4BENCH_READ_CCNT(*end);
}

static void
guest_run_bm(guest_t *self,
             guest_bm_stamp_getter_fn *stamp_getter_fn,
             vcpu_benchmark_results_t *r)
{
    memset(r, 0, sizeof(*r));

    for (int i = 0; i < VCPU_BENCH_N_ITERATIONS; i++) {
        bool is_anomaly;
        uint64_t start, end, elapsed;

        stamp_getter_fn(self, r, &start, &end);

        elapsed = end - start;

#if VCPU_BENCH_COLLECT_DEEP_DATA != 0
        r->deep_data.start[i] = start;
        r->deep_data.end[i] = end;
        r->deep_data.elapsed[i] = elapsed;
#endif

        if (i == 0) {
            /* The first result is always a big jump from 0 to N. */
            is_anomaly = 0;
        } else {
            is_anomaly = guest_bm_cycle_counter_anomaly_detected(self, i,
                                                                 elapsed, r);
        }

        r->complete_total += elapsed;
        if (!is_anomaly) {
            r->clipped_total += elapsed;
        } else {
            r->n_anomalies++;
        }
        r->min = MIN(r->min, elapsed);
        r->max = MAX(r->max, elapsed);
    }

    r->clipped_avg = guest_bm_calc_clipped_average(self, r);
    r->complete_avg = guest_bm_calc_complete_average(self, r);
}

static void
guest_report_bm_results(guest_t *self,
                        enum vcpu_benchmarks bm, vcpu_benchmark_results_t *r)
{
    vmm_call_report_end_results(self, bm,
                                r->min, r->max,
                                r->clipped_avg, r->complete_avg,
                                r->clipped_total, r->complete_total,
                                r->n_anomalies);

#if VCPU_BENCH_COLLECT_DEEP_DATA != 0
    for (int i = 0; i < VCPU_BENCH_N_ITERATIONS; i++) {
        vmm_call_report_deep_results(self, bm, i,
                                     r->deep_data.start[i],
                                     r->deep_data.end[i]);
    }
#endif
}

/* Don't want to store this on the stack because adjusting N_ITERATIONS will
 * expand its size and it could silently become larger than the stack can hold.
 */
static vcpu_benchmark_results_t guest_results;

static int
guest_run_benchmarks(guest_t *self)
{
    guest_run_bm(self, guest_run_bm_hvc_priv_escalate, &guest_results);
    guest_report_bm_results(self, VCPU_BENCHMARK_HVC_PRIV_ESCALATE,
                            &guest_results);

    guest_run_bm(self, guest_run_bm_eret_priv_deescalate, &guest_results);
    guest_report_bm_results(self, VCPU_BENCHMARK_ERET_PRIV_DESCALATE,
                            &guest_results);

    guest_run_bm(self, guest_run_bm_hvc_null_syscall, &guest_results);
    guest_report_bm_results(self, VCPU_BENCHMARK_HVC_NULL_SYSCALL,
                            &guest_results);

    guest_run_bm(self, guest_run_bm_call_syscall, &guest_results);
    guest_report_bm_results(self, VCPU_BENCHMARK_CALL_SYSCALL,
                            &guest_results);

    guest_run_bm(self, guest_run_bm_reply_syscall, &guest_results);
    guest_report_bm_results(self, VCPU_BENCHMARK_REPLY_SYSCALL,
                            &guest_results);

    return 0;
}

int
guest_main(guest_t *self)
{
    seL4_Word currentel, cpacr;
    seL4_Error err;

    cleanGuestContextToDCache(self, 0);
    cleanSharedGlobalsToDCache(0);
    guest_install_vector_table();

    currentel = get_currentel();

    vmm_printf(self, "Guest%d: shdata @%p, magic %x, CurrentEL: %lx, "
               "IPC buffer @%p\n",
               self->id, self, guests[1].magic, currentel,
               seL4_GetIPCBuffer());

    if (self->magic != VCPU_BENCH_SHDATA_MAGIC) {
        sel4_dbg_printf(self, "Guest%d: Magic %x is invalid!\n",
                        self->id, self->magic);
        return 0;
    }

    err = seL4_HypDebugCapIdentify(self->badged_fault_ep.capPtr);
    err = seL4_HypDebugCapIdentify(self->badged_fault_ep_ipc_copy.capPtr);

    sel4_dbg_printf(self, "GUEST%d: badged ep cap %d identified as %d, "
                    "and ipc cap %d identified as %d.\n",
                    self->id,
                    self->badged_fault_ep.capPtr,
                    seL4_HypDebugCapIdentify(self->badged_fault_ep.capPtr),
                    self->badged_fault_ep_ipc_copy.capPtr,
                    seL4_HypDebugCapIdentify(self->badged_fault_ep_ipc_copy.capPtr));

    if (!is_el1(currentel)) {
        vmm_printf(self, "Guest%d is not in EL1 as required!\n", self->id);
        vmm_call_exit_thread(self, false, -1, 0);
    }

    err = guest_run_benchmarks(self);

    // Done.
    vmm_call_exit_thread(self, false, 0, 0);
}
