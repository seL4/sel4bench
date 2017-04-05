/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
/* This is very much a work in progress IPC benchmarking set. Goal is
   to eventually use this to replace the rest of the random benchmarking
   happening in this app with just what we need */

#include <autoconf.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <string.h>
#include <utils/util.h>
#include <vka/vka.h>

#include <benchmark.h>
#include <ipc.h>

/* arch/ipc.h requires these defines */
#define NOPS ""

#include <arch/ipc.h>

#define NUM_ARGS 2
#define WARMUPS RUNS
#define OVERHEAD_RETRIES 4

typedef struct helper_thread {
    sel4utils_process_t process;
    seL4_CPtr ep;
    seL4_CPtr result_ep;
    char *argv[NUM_ARGS];
    char argv_strings[NUM_ARGS][WORD_STRING_SIZE];
} helper_thread_t;

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

static void
timing_init(void)
{
    sel4bench_init();

}

void
timing_destroy(void)
{
    sel4bench_destroy();
}

static void
send_result(seL4_CPtr ep, ccnt_t result)
{
    int length = sizeof(ccnt_t) / sizeof(seL4_Word);
    unsigned int shift = length > 1u ? seL4_WordBits : 0;
    for (int i = length - 1; i >= 0; i--) {
        seL4_SetMR(i, (seL4_Word) result);
        result = result >> shift;
    }

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length);
    seL4_Send(ep, tag);
}

static ccnt_t
get_result(seL4_CPtr ep)
{
    ccnt_t result = 0;
    seL4_MessageInfo_t tag = seL4_Recv(ep, NULL);
    int length = seL4_MessageInfo_get_length(tag);
    unsigned int shift = length > 1u ? seL4_WordBits : 0;

    for (int i = 0; i < length; i++) {
        result = result << shift;
        result += seL4_GetMR(i);
    }

    return result;
}

static inline void
dummy_seL4_Send(seL4_CPtr ep, seL4_MessageInfo_t tag)
{
    (void)ep;
    (void)tag;
}

static inline void
dummy_seL4_Call(seL4_CPtr ep, seL4_MessageInfo_t tag)
{
    (void)ep;
    (void)tag;
}

static inline void
dummy_seL4_Recv(seL4_CPtr ep, void *badge)
{
    (void)ep;
    (void)badge;
}

static inline void
dummy_seL4_Reply(seL4_MessageInfo_t tag)
{
    (void)tag;
}

seL4_Word ipc_call_func(int argc, char *argv[]);
seL4_Word ipc_call_func2(int argc, char *argv[]);
seL4_Word ipc_call_10_func(int argc, char *argv[]);
seL4_Word ipc_call_10_func2(int argc, char *argv[]);
seL4_Word ipc_replyrecv_func2(int argc, char *argv[]);
seL4_Word ipc_replyrecv_func(int argc, char *argv[]);
seL4_Word ipc_replyrecv_10_func2(int argc, char *argv[]);
seL4_Word ipc_replyrecv_10_func(int argc, char *argv[]);
seL4_Word ipc_send_func(int argc, char *argv[]);
seL4_Word ipc_recv_func(int argc, char *argv[]);

static helper_func_t bench_funcs[] = {
    ipc_call_func,
    ipc_call_func2,
    ipc_call_10_func,
    ipc_call_10_func2,
    ipc_replyrecv_func2,
    ipc_replyrecv_func,
    ipc_replyrecv_10_func2,
    ipc_replyrecv_10_func,
    ipc_send_func,
    ipc_recv_func
};

#define IPC_CALL_FUNC(name, bench_func, send_func, call_func, send_start_end, length) \
    seL4_Word name(int argc, char *argv[]) { \
    uint32_t i; \
    ccnt_t start UNUSED, end UNUSED; \
    seL4_CPtr ep = atoi(argv[0]);\
    seL4_CPtr result_ep = atoi(argv[1]);\
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length); \
    call_func(ep, tag); \
    COMPILER_MEMORY_FENCE(); \
    for (i = 0; i < WARMUPS; i++) { \
        READ_COUNTER_BEFORE(start); \
        bench_func(ep, tag); \
        READ_COUNTER_AFTER(end); \
    } \
    COMPILER_MEMORY_FENCE(); \
    send_result(result_ep, send_start_end); \
    send_func(ep, tag); \
    seL4_Recv(ep, NULL);/* block so we don't run off the stack */ \
    return 0; \
}

IPC_CALL_FUNC(ipc_call_func, DO_REAL_CALL, seL4_Send, dummy_seL4_Call, end, 0)
IPC_CALL_FUNC(ipc_call_func2, DO_REAL_CALL, dummy_seL4_Send, seL4_Call, start, 0)
IPC_CALL_FUNC(ipc_call_10_func, DO_REAL_CALL_10, seL4_Send, dummy_seL4_Call, end, 10)
IPC_CALL_FUNC(ipc_call_10_func2, DO_REAL_CALL_10, dummy_seL4_Send, seL4_Call, start, 10)

#define IPC_REPLY_RECV_FUNC(name, bench_func, reply_func, recv_func, send_start_end, length) \
seL4_Word name(int argc, char *argv[]) { \
    uint32_t i; \
    ccnt_t start UNUSED, end UNUSED; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length); \
    seL4_CPtr ep = atoi(argv[0]);\
    seL4_CPtr result_ep = atoi(argv[1]);\
    recv_func(ep, NULL); \
    COMPILER_MEMORY_FENCE(); \
    for (i = 0; i < WARMUPS; i++) { \
        READ_COUNTER_BEFORE(start); \
        bench_func(ep, tag); \
        READ_COUNTER_AFTER(end); \
    } \
    COMPILER_MEMORY_FENCE(); \
    send_result(result_ep, send_start_end); \
    reply_func(tag); \
    seL4_Recv(ep, NULL); /* block so we don't run off the stack */ \
    return 0; \
}

IPC_REPLY_RECV_FUNC(ipc_replyrecv_func2, DO_REAL_REPLY_RECV, seL4_Reply, seL4_Recv, end, 0)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_func, DO_REAL_REPLY_RECV, dummy_seL4_Reply, seL4_Recv, start, 0)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_10_func2, DO_REAL_REPLY_RECV_10, seL4_Reply, seL4_Recv, end, 10)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_10_func, DO_REAL_REPLY_RECV_10, dummy_seL4_Reply, seL4_Recv, start, 10)

seL4_Word
ipc_recv_func(int argc, char *argv[])
{
    uint32_t i;
    ccnt_t start UNUSED, end UNUSED;
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    COMPILER_MEMORY_FENCE();
    for (i = 0; i < WARMUPS; i++) {
        READ_COUNTER_BEFORE(start);
        DO_REAL_RECV(ep);
        READ_COUNTER_AFTER(end);
    }
    COMPILER_MEMORY_FENCE();
    DO_REAL_RECV(ep);
    send_result(result_ep, end);
    return 0;
}

seL4_Word
ipc_send_func(int argc, char *argv[])
{
    uint32_t i;
    ccnt_t start UNUSED, end UNUSED;
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    COMPILER_MEMORY_FENCE();
    for (i = 0; i < WARMUPS; i++) {
        READ_COUNTER_BEFORE(start);
        DO_REAL_SEND(ep, tag);
        READ_COUNTER_AFTER(end);
    }
    COMPILER_MEMORY_FENCE();
    send_result(result_ep, start);
    DO_REAL_SEND(ep, tag);
    return 0;
}

#define MEASURE_OVERHEAD(op, dest, decls) do { \
    uint32_t i; \
    timing_init(); \
    for (i = 0; i < OVERHEAD_RETRIES; i++) { \
        uint32_t j; \
        for (j = 0; j < RUNS; j++) { \
            uint32_t k; \
            decls; \
            ccnt_t start, end; \
            COMPILER_MEMORY_FENCE(); \
            for (k = 0; k < WARMUPS; k++) { \
                READ_COUNTER_BEFORE(start); \
                op; \
                READ_COUNTER_AFTER(end); \
            } \
            COMPILER_MEMORY_FENCE(); \
            dest[j] = end - start; \
        } \
        if (results_stable(dest, RUNS)) break; \
    } \
    timing_destroy(); \
} while(0)

static void
measure_overhead(ipc_results_t *results)
{
    MEASURE_OVERHEAD(DO_NOP_CALL(0, tag),
                     results->overhead_benchmarks[CALL_OVERHEAD],
                     seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0));
    MEASURE_OVERHEAD(DO_NOP_REPLY_RECV(0, tag),
                     results->overhead_benchmarks[REPLY_RECV_OVERHEAD],
                     seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0));
    MEASURE_OVERHEAD(DO_NOP_SEND(0, tag),
                     results->overhead_benchmarks[SEND_OVERHEAD],
                     seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0));
    MEASURE_OVERHEAD(DO_NOP_RECV(0),
                     results->overhead_benchmarks[RECV_OVERHEAD],
                     {});
    MEASURE_OVERHEAD(DO_NOP_CALL_10(0, tag10),
                     results->overhead_benchmarks[CALL_10_OVERHEAD],
                     seL4_MessageInfo_t tag10 = seL4_MessageInfo_new(0, 0, 0, 10));
    MEASURE_OVERHEAD(DO_NOP_REPLY_RECV_10(0, tag10),
                     results->overhead_benchmarks[REPLY_RECV_10_OVERHEAD],
                     seL4_MessageInfo_t tag10 = seL4_MessageInfo_new(0, 0, 0, 10));
}

/* this function is never exeucted, it just lives in the scheduler queue */
static NORETURN seL4_Word
dummy_fn(int argc, char *argv[])
{
    while (1);
}

void
run_bench(env_t *env, cspacepath_t result_ep_path,
          const benchmark_params_t *params,
          ccnt_t *ret1, ccnt_t *ret2,
          helper_thread_t *client, helper_thread_t *server)
{

    timing_init();

    /* start processes */
    if (sel4utils_spawn_process(&client->process, &env->slab_vka, &env->vspace, NUM_ARGS, client->argv, 1)) {
        ZF_LOGF("Failed to spawn client\n");
    }

    if (sel4utils_spawn_process(&server->process, &env->slab_vka, &env->vspace, NUM_ARGS, server->argv, 1)) {
        ZF_LOGF("Failed to spawn server\n");
    }

    /* recv for results */
    *ret1 = get_result(result_ep_path.capPtr);
    *ret2 = get_result(result_ep_path.capPtr);

    /* clean up - clean server first in case it is sharing the client's cspace and vspace */
    seL4_TCB_Suspend(client->process.thread.tcb.cptr);
    seL4_TCB_Suspend(server->process.thread.tcb.cptr);

    timing_destroy();
}

int
main(int argc, char **argv)
{
    env_t *env;
    vka_object_t ep, result_ep;
    cspacepath_t ep_path, result_ep_path;

    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 4,
        [seL4_EndpointObject] = 2,
    };

    env = benchmark_get_env(argc, argv, sizeof(ipc_results_t), object_freq);
    ipc_results_t *results = (ipc_results_t *) env->results;

    /* allocate benchmark endpoint - the IPC's that we benchmark
       will be sent over this ep */
    if (vka_alloc_endpoint(&env->slab_vka, &ep) != 0) {
        ZF_LOGF("Failed to allocate endpoint");
    }
    vka_cspace_make_path(&env->slab_vka, ep.cptr, &ep_path);

    /* allocate result ep - the IPC threads will send their timestamps
       to this ep */
    if (vka_alloc_endpoint(&env->slab_vka, &result_ep) != 0) {
        ZF_LOGF("Failed to allocate endpoint");
    }
    vka_cspace_make_path(&env->slab_vka, result_ep.cptr, &result_ep_path);

    /* measure benchmarking overhead */
    measure_overhead(results);

    helper_thread_t client, server_thread, server_process, dummy;

    benchmark_shallow_clone_process(env, &client.process, seL4_MinPrio, 0, "client");
    benchmark_shallow_clone_process(env, &server_process.process, seL4_MinPrio, 0, "server process");
    benchmark_configure_thread_in_process(env, &client.process, &server_thread.process, seL4_MinPrio, 0, "server thread");
    benchmark_shallow_clone_process(env, &dummy.process, seL4_MinPrio, dummy_fn, "dummy");

    if (sel4utils_spawn_process(&dummy.process, &env->slab_vka, &env->vspace, 0, NULL, 1)) {
        ZF_LOGF("Failed to spawn dummy process\n");
    }

    client.ep = sel4utils_copy_cap_to_process(&client.process, ep_path);
    client.result_ep = sel4utils_copy_cap_to_process(&client.process, result_ep_path);

    server_process.ep = sel4utils_copy_cap_to_process(&server_process.process, ep_path);
    server_process.result_ep = sel4utils_copy_cap_to_process(&server_process.process, result_ep_path);

    server_thread.ep = client.ep;
    server_thread.result_ep = client.result_ep;

    sel4utils_create_word_args(client.argv_strings, client.argv, NUM_ARGS, client.ep, client.result_ep);
    sel4utils_create_word_args(server_process.argv_strings, server_process.argv, NUM_ARGS,
                                server_process.ep, server_process.result_ep);
    sel4utils_create_word_args(server_thread.argv_strings, server_thread.argv, NUM_ARGS,
                                server_thread.ep, server_thread.result_ep);

    /* run the benchmark */
    ccnt_t start, end;
    for (int i = 0; i < RUNS; i++) {
        int j;
        ZF_LOGI("--------------------------------------------------\n");
        ZF_LOGI("Doing iteration %d\n", i);
        ZF_LOGI("--------------------------------------------------\n");
        for (j = 0; j < ARRAY_SIZE(benchmark_params); j++) {
            const struct benchmark_params* params = &benchmark_params[j];
            ZF_LOGI("%s\t: IPC duration (%s), client prio: %3d server prio %3d, %s vspace, length %2d\n",
                    params->name,
                    params->direction == DIR_TO ? "client --> server" : "server --> client",
                    params->client_prio, params->server_prio,
                    params->same_vspace ? "same" : "diff", params->length);

            /* set up client for benchmark */
            int error = seL4_TCB_SetPriority(client.process.thread.tcb.cptr, params->client_prio);
            assert(error == seL4_NoError);
            client.process.entry_point = bench_funcs[params->client_fn];

            /* set up dummy for benchmark */
            if (params->dummy_thread) {
                error = seL4_TCB_SetPriority(client.process.thread.tcb.cptr, params->dummy_prio);
                assert(error == seL4_NoError);
            }

            if (params->same_vspace) {
                error = seL4_TCB_SetPriority(server_thread.process.thread.tcb.cptr, params->server_prio);
                assert(error == seL4_NoError);
                server_thread.process.entry_point = bench_funcs[params->server_fn];
            } else {
                error = seL4_TCB_SetPriority(server_process.process.thread.tcb.cptr, params->server_prio);
                assert(error == seL4_NoError);
                server_process.process.entry_point = bench_funcs[params->server_fn];
            }

            run_bench(env, result_ep_path, params, &end, &start, &client,
                      params->same_vspace ? &server_thread : &server_process);

            if (end > start) {
                results->benchmarks[j][i] = end - start;
            } else {
                results->benchmarks[j][i] = start - end;
            }
        }
    }

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

