/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

//#define ZF_LOG_LEVEL ZF_LOG_VERBOSE
#include <autoconf.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>
#include <sel4utils/thread.h>
#include <sel4utils/util.h>
#include <utils/ud.h>

#define NOPS ""

#include <benchmark.h>
#include <ipc.h>
#include <arch/ipc.h>
#include <timeout.h>
#include <sel4_arch/timeout.h>

#define N_REGISTERS (sizeof(seL4_UserContext) / sizeof(seL4_Word))

static volatile ccnt_t timestamp;

void
abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

void
__arch_putchar(int c)
{
    benchmark_putchar(c);
}

void
client_fn(seL4_CPtr ep)
{
    /* just keep calling the server */
    while (1) {
        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
    }
}

void
server_fn(seL4_CPtr ntfn, seL4_CPtr ep)
{
    /* signal initialiser that we are ready,
     * and wait for client request */
    seL4_SignalRecv(ntfn, ep, NULL);
    while (1) {
        /* just keep reading the timestamp until a fault occurs */
        SEL4BENCH_READ_CCNT(timestamp);
    }
}

static inline void
seL4_TimeoutReply(seL4_UserContext regs)
{
    seL4_Word *word_regs = (seL4_Word *) &regs;
    for (int i = seL4_FastMessageRegisters; i < N_REGISTERS; i++) {
        seL4_SetMR(i, word_regs[i]);
    }

    seL4_TimeoutReplyWithMRs(regs);
}

void
benchmark_timeout(env_t *env, timeout_results_t *results)
{
    /* allocate client-server endpoint */
    vka_object_t ep = {0};
    int error = vka_alloc_endpoint(&env->vka, &ep);
    assert(error == 0);

    /* allocate notification */
    vka_object_t ntfn = {0};
    error = vka_alloc_notification(&env->vka, &ntfn);
    assert(error == 0);

    /* allocate timeout fault endpoint */
    vka_object_t tfep = {0};
    error = vka_alloc_endpoint(&env->vka, &tfep);
    assert(error == 0);

    /* create client */
    sel4utils_thread_t client;
    benchmark_configure_thread(env, seL4_CapNull, 1, "client", &client);
    /* change client sched params */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), client.sched_context.cptr,
                                        10 * US_IN_MS, 10 * US_IN_MS, 0);
    assert(error == 0);

    sel4utils_start_thread(&client, client_fn, (void *) (ep.cptr), NULL, true);
    ZF_LOGF_IFERR(error, "Failed to start client");

    /* create server */
    seL4_CapData_t guard = seL4_CapData_Guard_new(0, seL4_WordBits - CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    sel4utils_thread_t server;
    sel4utils_thread_config_t config = {
        .priority = 2,
        .cspace = SEL4UTILS_CNODE_SLOT,
        .cspace_root_data = guard,
        .timeout_fault_endpoint = tfep.cptr,
        .create_sc = true,
    };
    error = sel4utils_configure_thread_config(&env->simple, &env->vka, &env->vspace,
                                              &env->vspace, config, &server);
    ZF_LOGF_IFERR(error, "Failed to configure server");
    sel4utils_start_thread(&server, server_fn, (void *) ntfn.cptr, (void *) ep.cptr, true);

    ZF_LOGD("Wait for server init\n");
    seL4_Wait(ntfn.cptr, NULL);

    ZF_LOGD("Checkpoint server\n");
    seL4_UserContext server_state = {0};
    error = seL4_TCB_ReadRegisters(server.tcb.cptr, false, 0,
                                   sizeof(seL4_UserContext) / sizeof(seL4_Word), &server_state);
    ZF_LOGF_IFERR(error, "Failed to read registers");

    ZF_LOGD("Convert server to passive\n");
    error = seL4_SchedContext_Unbind(server.sched_context.cptr);
    ZF_LOGF_IFERR(error, "Failed to convert server to passive");

    /* BENCHMARK START */
    for (int i = 0; i < N_RUNS; i++) {
        ccnt_t got_fault, sent_reply;
        ZF_LOGD("Wait for temporal fault");
        seL4_Recv(tfep.cptr, NULL);
        ZF_LOGD("Got temporal fault");
        SEL4BENCH_READ_CCNT(got_fault);
        results->server_to_handler[i] = got_fault - timestamp;

        /* restore server */
        error = seL4_SchedContext_Bind(server.sched_context.cptr, server.tcb.cptr);
        assert(error == seL4_NoError);

        ZF_LOGD("Reply to server");
        seL4_TimeoutReply(server_state);

        ZF_LOGD("Wait for server to init server");
        seL4_RecvNoMRs(ntfn.cptr);

        ZF_LOGD("Convert back to passive");
        error = seL4_SchedContext_Unbind(server.sched_context.cptr);
        ZF_LOGF_IFERR(error, "Failed to convert server to passive");
        SEL4BENCH_READ_CCNT(sent_reply);
        results->handle_timeout[i] =  sent_reply - got_fault;

        ZF_LOGD("Restart client");
        error = seL4_TCB_Resume(client.tcb.cptr);
        assert(error == seL4_NoError);
    }
}

static inline int
seL4_SchedContext_Bind_Nop(seL4_SchedContext service, seL4_CPtr cap)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(SchedContextBind, 0, 1, 0);

    /* Setup input capabilities. */
    seL4_SetCap(0, cap);

    /* Perform the call. */
    DO_NOP_CALL(service, tag);

    return seL4_MessageInfo_get_label(tag);
}

static inline int
seL4_SchedContext_Unbind_Nop(seL4_SchedContext service)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(SchedContextUnbind, 0, 0, 0);

    /* Perform the call. */
    DO_NOP_CALL(service, tag);

    return seL4_MessageInfo_get_label(tag);
}

static inline void
seL4_TimeoutReply_Nop(seL4_UserContext regs)
{
    seL4_Word *word_regs = (seL4_Word *) &regs;
    UNUSED seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, N_REGISTERS);
    for (int i = seL4_FastMessageRegisters; i < N_REGISTERS; i++) {
        seL4_SetMR(i, word_regs[i]);
    }

    DO_NOP_REPLY_10(info);
}

void
measure_timeout_overhead(timeout_results_t *results)
{
    ccnt_t start, end;
    /* measure overhead of cycle count read */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results->ccnt_overhead[i] = (end - start);
    }

	seL4_CPtr cap1 = 0;
    seL4_CPtr cap2 = 1;
    /* measure overhead of stub seL4_SchedContext_Bind, seL4_RecvWithMRs and seL4_TimeoutReply +
     * read ccnt */
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        seL4_SchedContext_Bind_Nop(cap1, cap2);
		seL4_UserContext regs = {0};
        seL4_TimeoutReply_Nop(regs);
        DO_NOP_RECV(cap1);
        seL4_SchedContext_Unbind_Nop(cap1);
        SEL4BENCH_READ_CCNT(end);
        results->handle_timeout_overhead[i] = (end - start);
    }
}

int
main(int argc, char **argv)
{
    env_t *env;
    UNUSED int error;
    timeout_results_t *results;

    env = benchmark_get_env(argc, argv, sizeof(timeout_results_t));
    results = (timeout_results_t *) env->results;

    sel4bench_init();

    measure_timeout_overhead(results);
    benchmark_timeout(env, results);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

