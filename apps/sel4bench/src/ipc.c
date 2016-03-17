/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
/* This is very much a work in progress IPC benchmarking set. Goal is
   to eventually use this to replace the rest of the random benchmarking
   happening in this app with just what we need */

#include <autoconf.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <simple/simple.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <string.h>
#include <utils/ansi.h>
#include <vka/vka.h>

#include "benchmark.h"
#include "math.h"
#include "timing.h"
#include "processing.h"
#include "printing.h"

/* ipc.h requires these defines */
#define __SWINUM(x) ((x) & 0x00ffffff)
#define NOPS ""

#include <arch/ipc.h>

#define NUM_ARGS 2
#define WARMUPS 16
#define RUNS 16
#define OVERHEAD_RETRIES 4


/* The fence is designed to try and prevent the compiler optimizing across code boundaries
   that we don't want it to. The reason for preventing optimization is so that things like
   overhead calculations aren't unduly influenced */
#define FENCE() asm volatile("" ::: "memory")


#define OVERHEAD_BENCH_PARAMS(n) { .name = n }

enum overhead_benchmarks {
    CALL_OVERHEAD,
    REPLY_RECV_OVERHEAD,
    SEND_OVERHEAD,
    RECV_OVERHEAD,
    CALL_10_OVERHEAD,
    REPLY_RECV_10_OVERHEAD,
    /******/
    NOVERHEADBENCHMARKS
};

enum overheads {
    CALL_REPLY_RECV_OVERHEAD,
    CALL_REPLY_RECV_10_OVERHEAD,
    SEND_RECV_OVERHEAD,
    /******/
    NOVERHEADS
};

typedef enum dir {
    /* client ---> server */
    DIR_TO,
    /* server --> client */
    DIR_FROM
} dir_t;

typedef struct benchmark_params {
    /* name of the function we are benchmarking */
    const char* name;
    /* direction of the ipc */
    dir_t direction;
    /* functions for client and server to run */
    helper_func_t server_fn, client_fn;
    /* should client and server run in the same vspace? */
    bool same_vspace;
    /* prio for client and server to run at */
    uint8_t server_prio, client_prio;
    /* length of ipc to send */
    uint8_t length;
    /* id of overhead calculation for this function */
    enum overheads overhead_id;
    /* should we put a dummy thread in the scheduler? */
    bool dummy_thread;
    /* if so, what prio should the dummy thread be at? */
    uint8_t dummy_prio;
} benchmark_params_t;

struct overhead_benchmark_params {
    const char* name;
};

typedef struct helper_thread {
    sel4utils_process_config_t config;
    sel4utils_process_t process;
    seL4_CPtr ep;
    seL4_CPtr result_ep;
    char *argv[NUM_ARGS];
    char argv_strings[NUM_ARGS][WORD_STRING_SIZE];
} helper_thread_t;

uint32_t ipc_call_func(int argc, char *argv[]);
uint32_t ipc_call_func2(int argc, char *argv[]);
uint32_t ipc_call_10_func(int argc, char *argv[]);
uint32_t ipc_call_10_func2(int argc, char *argv[]);
uint32_t ipc_replyrecv_func2(int argc, char *argv[]);
uint32_t ipc_replyrecv_func(int argc, char *argv[]);
uint32_t ipc_replyrecv_10_func2(int argc, char *argv[]);
uint32_t ipc_replyrecv_10_func(int argc, char *argv[]);
uint32_t ipc_send_func(int argc, char *argv[]);
uint32_t ipc_recv_func(int argc, char *argv[]);

/* array of benchmarks to run */
/* one way IPC benchmarks - varying size, direction and priority.*/
static const benchmark_params_t benchmark_params[] = {
    /* Call fastpath between client and server in the same address space */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_func2,
        .server_fn   = ipc_replyrecv_func2,
        .same_vspace = true,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* ReplyRecv fastpath between server and client in the same address space */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_func,
        .server_fn   = ipc_replyrecv_func,
        .same_vspace = true,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* Call faspath between client and server in different address spaces */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_func2,
        .server_fn   = ipc_replyrecv_func2,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* ReplyRecv fastpath between server and client in different address spaces */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_func,
        .server_fn   = ipc_replyrecv_func,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* Call fastpath, low prio client to high prio server in different address space */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_func2,
        .client_fn   = ipc_call_func2,
        .server_fn   = ipc_replyrecv_func2,
        .same_vspace = false,
        .client_prio = seL4_MinPrio,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* ReplyRecv slowpath, high prio server to low prio client, different address space */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_func,
        .server_fn   = ipc_replyrecv_func,
        .same_vspace = false,
        .client_prio = seL4_MinPrio,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* Call slowpath, high prio client to low prio server, different address space */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_func2,
        .server_fn   = ipc_replyrecv_func2,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MinPrio,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
    /* ReplyRecv fastpath, low prio server to high prio client, different address space */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_func,
        .server_fn   = ipc_replyrecv_func,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MinPrio,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD
    },
     /* ReplyRecv slowpath, high prio server to low prio client, different address space, with
      * low prio dummy thread also in scheduler */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_func,
        .server_fn   = ipc_replyrecv_func,
        .same_vspace = false,
        .client_prio = seL4_MinPrio + 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD,
        .dummy_thread = true,
        .dummy_prio = seL4_MinPrio, 
    },
    /* Call slowpath, high prio client to low prio server, different address space, with 
     * low prio dummy thread also in scheduler */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_func2,
        .server_fn   = ipc_replyrecv_func2,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MinPrio + 1,
        .length = 0,
        .overhead_id = CALL_REPLY_RECV_OVERHEAD,
        .dummy_thread = true,
        .dummy_prio = seL4_MinPrio, 
    },
    /* Send slowpath (no fastpath for send) same prio client-server, different address space */
    {
        .name        = "seL4_Send",
        .direction   = DIR_TO,
        .client_fn   = ipc_send_func,
        .server_fn   = ipc_recv_func,
        .same_vspace = false,
        .client_prio = 100,
        .server_prio = 100,
        .length = 0,
        .overhead_id = SEND_RECV_OVERHEAD
    },
    /* Call slowpath, long IPC (10), same prio client to server, different address space */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = ipc_call_10_func2,
        .server_fn   = ipc_replyrecv_10_func2,
        .same_vspace = false,
        .client_prio = 100,
        .server_prio = 100,
        .length = 10,
        .overhead_id = CALL_REPLY_RECV_10_OVERHEAD
    },
    /* ReplyRecv slowpath, long IPC (10), same prio server to client, on the slowpath, different address space */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = ipc_call_10_func,
        .server_fn   = ipc_replyrecv_10_func,
        .same_vspace = false,
        .client_prio = 100,
        .server_prio = 100,
        .length = 10,
        .overhead_id = CALL_REPLY_RECV_10_OVERHEAD
    }
};

static const struct overhead_benchmark_params overhead_benchmark_params[] = {
    [CALL_OVERHEAD]          = {"call"},
    [REPLY_RECV_OVERHEAD]    = {"reply recv"},
    [SEND_OVERHEAD]          = {"send"},
    [RECV_OVERHEAD]          = {"recv"},
    [CALL_10_OVERHEAD]       = {"call"},
    [REPLY_RECV_10_OVERHEAD] = {"reply recv"},
};

typedef struct ipc_results {
    /* Raw results from benchmarking. These get checked for sanity */
    ccnt_t overhead_benchmarks[NOVERHEADBENCHMARKS][RUNS];
    ccnt_t benchmarks[ARRAY_SIZE(benchmark_params)][RUNS];
    /* A worst case overhead */
    ccnt_t overheads[NOVERHEADS];
    /* Calculated results to print out */
    result_t results[ARRAY_SIZE(benchmark_params)];
} ipc_results_t;

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
        result += seL4_GetMR(0);
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

#define IPC_CALL_FUNC(name, bench_func, send_func, call_func, send_start_end, length) \
uint32_t name(int argc, char *argv[]) { \
    uint32_t i; \
    ccnt_t start UNUSED, end UNUSED; \
    seL4_CPtr ep = atoi(argv[0]);\
    seL4_CPtr result_ep = atoi(argv[1]);\
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length); \
    call_func(ep, tag); \
    FENCE(); \
    for (i = 0; i < WARMUPS; i++) { \
        READ_COUNTER_BEFORE(start); \
        bench_func(ep, tag); \
        READ_COUNTER_AFTER(end); \
    } \
    FENCE(); \
    send_result(result_ep, send_start_end); \
    send_func(ep, tag); \
    return 0; \
}

IPC_CALL_FUNC(ipc_call_func, DO_REAL_CALL, seL4_Send, dummy_seL4_Call, end, 0)
IPC_CALL_FUNC(ipc_call_func2, DO_REAL_CALL, dummy_seL4_Send, seL4_Call, start, 0)
IPC_CALL_FUNC(ipc_call_10_func, DO_REAL_CALL_10, seL4_Send, dummy_seL4_Call, end, 10)
IPC_CALL_FUNC(ipc_call_10_func2, DO_REAL_CALL_10, dummy_seL4_Send, seL4_Call, start, 10)

#define IPC_REPLY_RECV_FUNC(name, bench_func, reply_func, recv_func, send_start_end, length) \
uint32_t name(int argc, char *argv[]) { \
    uint32_t i; \
    ccnt_t start UNUSED, end UNUSED; \
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length); \
    seL4_CPtr ep = atoi(argv[0]);\
    seL4_CPtr result_ep = atoi(argv[1]);\
    recv_func(ep, NULL); \
    FENCE(); \
    for (i = 0; i < WARMUPS; i++) { \
        READ_COUNTER_BEFORE(start); \
        bench_func(ep, tag); \
        READ_COUNTER_AFTER(end); \
    } \
    FENCE(); \
    send_result(result_ep, send_start_end); \
    reply_func(tag); \
    return 0; \
}

IPC_REPLY_RECV_FUNC(ipc_replyrecv_func2, DO_REAL_REPLY_RECV, seL4_Reply, seL4_Recv, end, 0)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_func, DO_REAL_REPLY_RECV, dummy_seL4_Reply, seL4_Recv, start, 0)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_10_func2, DO_REAL_REPLY_RECV_10, seL4_Reply, seL4_Recv, end, 10)
IPC_REPLY_RECV_FUNC(ipc_replyrecv_10_func, DO_REAL_REPLY_RECV_10, dummy_seL4_Reply, seL4_Recv, start, 10)

uint32_t
ipc_recv_func(int argc, char *argv[])
{
    uint32_t i;
    ccnt_t start UNUSED, end UNUSED;
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    FENCE();
    for (i = 0; i < WARMUPS; i++) {
        READ_COUNTER_BEFORE(start);
        DO_REAL_RECV(ep);
        READ_COUNTER_AFTER(end);
    }
    FENCE();
    DO_REAL_RECV(ep);
    send_result(result_ep, end);
    return 0;
}

uint32_t
ipc_send_func(int argc, char *argv[])
{
    uint32_t i;
    ccnt_t start UNUSED, end UNUSED;
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    FENCE();
    for (i = 0; i < WARMUPS; i++) {
        READ_COUNTER_BEFORE(start);
        DO_REAL_SEND(ep, tag);
        READ_COUNTER_AFTER(end);
    }
    FENCE();
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
            FENCE(); \
            for (k = 0; k < WARMUPS; k++) { \
                READ_COUNTER_BEFORE(start); \
                op; \
                READ_COUNTER_AFTER(end); \
            } \
            FENCE(); \
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

void
init_config(env_t env, helper_thread_t *thread, helper_func_t thread_fn, int prio)
{
    /* set up a process that runs in its own address space */
    bzero(&thread->config, sizeof(thread->config));
    thread->config.is_elf = false;
    thread->config.create_cspace = true;
    thread->config.one_level_cspace_size_bits = CONFIG_SEL4UTILS_CSPACE_SIZE_BITS;
    thread->config.create_vspace = true;
    thread->config.reservations = &env->region;
    thread->config.num_reservations = 1;
    thread->config.create_fault_endpoint = false;
    thread->config.fault_endpoint.cptr = 0; /* benchmark threads do not have fault eps */
    thread->config.priority = prio;
    thread->config.entry_point = thread_fn;
#ifndef CONFIG_KERNEL_STABLE
    thread->config.asid_pool = simple_get_init_cap(&env->simple, seL4_CapInitThreadASIDPool);
#endif

}

void
init_server_config(env_t env, helper_thread_t *server, helper_func_t server_fn, int prio,
                   helper_thread_t *client, int same_vspace)
{
    /* set up a server process which may share its address space with the client */
    server->config = client->config;
    server->config.priority = prio;
    server->config.entry_point = server_fn;

    if (same_vspace) {
        server->config.create_cspace = false;
        server->config.cnode = client->process.cspace;
        server->config.create_vspace = false;
        server->config.vspace = &client->process.vspace;
    }
}

/* this function is never exeucted, it just lives in the scheduler queue */
static NORETURN seL4_Word 
dummy_fn(int argc, char *argv[]) {
    while(1);
}

void
run_bench(env_t env, const benchmark_params_t *params, ccnt_t *ret1, ccnt_t *ret2)
{
    helper_thread_t client, server, dummy;

    timing_init();

    /* configure processes */
    init_config(env, &client, params->client_fn, params->client_prio);

    if (sel4utils_configure_process_custom(&client.process, &env->vka, &env->vspace, client.config)) {
        ZF_LOGF("Failed to configure client\n");
    }

    init_server_config(env, &server, params->server_fn, params->server_prio, &client, params->same_vspace);

    if (sel4utils_configure_process_custom(&server.process, &env->vka, &env->vspace, server.config)) {
        ZF_LOGF("Failed to configure server\n");
    }

    /* clone the text segment into the vspace - note that as we are only cloning the text
     * segment, you will not be able to use anything that relies on initialisation in benchmark
     * threads - like printf, (but seL4_Debug_PutChar is ok)
     */
    if (sel4utils_bootstrap_clone_into_vspace(&env->vspace, &client.process.vspace, env->region.reservation)) {
        ZF_LOGF("Failed to bootstrap client\n");
    }

    if (!params->same_vspace) {
        if (sel4utils_bootstrap_clone_into_vspace(&env->vspace, &server.process.vspace, env->region.reservation)) {
            ZF_LOGF("Failed to bootstrap server\n");
        }
    }

    if (params->dummy_thread) {
        init_config(env, &dummy, dummy_fn, params->dummy_prio);
        if (sel4utils_configure_process_custom(&dummy.process, &env->vka, &env->vspace, dummy.config)) {
            ZF_LOGF("Failed to configure dummy\n");
        }
        if (sel4utils_bootstrap_clone_into_vspace(&env->vspace, &dummy.process.vspace, env->region.reservation)) {
            ZF_LOGF("Failed to bootstrap dummy thread\n");
        }
        if (sel4utils_spawn_process(&dummy.process, &env->vka, &env->vspace, 0, NULL, 1)) {
            ZF_LOGF("Failed to spawn dummy process\n");
        }
    }   

    /* copy endpoint cptrs into a and b's respective cspaces*/
    client.ep = sel4utils_copy_cap_to_process(&client.process, env->ep_path);
    client.result_ep = sel4utils_copy_cap_to_process(&client.process, env->result_ep_path);

    if (!params->same_vspace) {
        server.ep = sel4utils_copy_cap_to_process(&server.process, env->ep_path);
        server.result_ep = sel4utils_copy_cap_to_process(&server.process, env->result_ep_path);
    } else {
        server.ep = client.ep;
        server.result_ep = client.result_ep;
    }

    /* set up args */
    sel4utils_create_word_args(client.argv_strings, client.argv, NUM_ARGS, client.ep, client.result_ep);
    sel4utils_create_word_args(server.argv_strings, server.argv, NUM_ARGS, server.ep, server.result_ep);

    /* start processes */
    if (sel4utils_spawn_process(&client.process, &env->vka, &env->vspace, NUM_ARGS, client.argv, 1)) {
        ZF_LOGF("Failed to spawn client\n");
    }

    if (sel4utils_spawn_process(&server.process, &env->vka, &env->vspace, NUM_ARGS, server.argv, 1)) {
        ZF_LOGF("Failed to spawn server\n");
    }

    /* recv for results */
    *ret1 = get_result(env->result_ep.cptr);
    *ret2 = get_result(env->result_ep.cptr);

    /* clean up - clean server first in case it is sharing the client's cspace and vspace */
    sel4utils_destroy_process(&server.process, &env->vka);
    sel4utils_destroy_process(&client.process, &env->vka);
    if (params->dummy_thread) {
        sel4utils_destroy_process(&dummy.process, &env->vka);
    }

    timing_destroy();
}

static int
check_overhead(ipc_results_t *results)
{
    ccnt_t overhead[NOVERHEADBENCHMARKS];
    int i;
    for (i = 0; i < NOVERHEADBENCHMARKS; i++) {
        if (!results_stable(results->overhead_benchmarks[i], RUNS)) {
            printf("Benchmarking overhead of a %s is not stable! Cannot continue\n", overhead_benchmark_params[i].name);
            print_all(results->overhead_benchmarks[i], RUNS);
#ifndef ALLOW_UNSTABLE_OVERHEAD
            return 0;
#endif
        }
        overhead[i] = results_min(results->overhead_benchmarks[i], RUNS);
    }
    /* Take the smallest overhead to be our benchmarking overhead */
    results->overheads[CALL_REPLY_RECV_OVERHEAD] = MIN(overhead[CALL_OVERHEAD], overhead[REPLY_RECV_OVERHEAD]);
    results->overheads[SEND_RECV_OVERHEAD] = MIN(overhead[SEND_OVERHEAD], overhead[RECV_OVERHEAD]);
    results->overheads[CALL_REPLY_RECV_10_OVERHEAD] = MIN(overhead[CALL_10_OVERHEAD], overhead[REPLY_RECV_10_OVERHEAD]);
    return 1;
}

static int
process_results(ipc_results_t *results)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(results->results); i++) {
        results->results[i] = process_result(results->benchmarks[i], RUNS, benchmark_params[i].name);
    }
    return 1;
}

/* for pasting into a spreadsheet or parsing */
static void
print_results_tsv(ipc_results_t *results)
{

    printf("Function\tDirection\tClient Prio\tServer Prio\tSame vspace?\tDummy (prio)?\tLength");
    print_result_header();
    for (int i = 0; i < ARRAY_SIZE(results->results); i++) {
        printf("%s\t", benchmark_params[i].name);
        printf("%s\t", benchmark_params[i].direction == DIR_TO ? "client -> server" : "server -> client");
        printf("%d\t", benchmark_params[i].client_prio);
        printf("%d\t", benchmark_params[i].server_prio);
        printf("%s\t", benchmark_params[i].same_vspace ? "true" : "false");
        printf("%s (%d)\t", benchmark_params[i].dummy_thread ? "true" : "false", benchmark_params[i].dummy_prio);
        print_result(&results->results[i]);
    }
}

static void
single_xml_result(int result, ccnt_t value, char *name)
{

    printf("\t<result name=\"");
    printf("%sAS", benchmark_params[result].same_vspace ? "Intra" : "Inter");
    printf("-%s", benchmark_params[result].name);
    printf("(%d %s %d, size %d)\" ", benchmark_params[result].client_prio,
           benchmark_params[result].direction == DIR_TO ? "--&gt;" : "&lt;--",
           benchmark_params[result].server_prio, benchmark_params[result].length);
    printf("type=\"%s\">"CCNT_FORMAT"</result>\n", name, value);

}


/* for bamboo */
static void
print_results_xml(ipc_results_t *results)
{
    int i;
    printf("<results>\n");
    for (i = 0; i < ARRAY_SIZE(results->results); i++) {
        single_xml_result(i, results->results[i].min, "min");
        single_xml_result(i, results->results[i].max, "max");
        single_xml_result(i, (ccnt_t) results->results[i].mean, "mean");
        single_xml_result(i, (ccnt_t) results->results[i].stddev, "stdev");
    }
    printf("</results>\n");
}

void
ipc_benchmarks_new(struct env* env)
{
    uint32_t i;
    ipc_results_t results;
    ccnt_t start, end;
    measure_overhead(&results);
    if (!check_overhead(&results)) {
        return;
    }

    for (i = 0; i < RUNS; i++) {
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
            run_bench(env, params, &end, &start);
            if (end > start) {
                results.benchmarks[j][i] = end - start;
            } else {
                results.benchmarks[j][i] = start - end;
            }
            results.benchmarks[j][i] -= results.overheads[params->overhead_id];
        }
    }
    if (!process_results(&results)) {
        return;
    }
    printf("--------------------------------------------------\n");
    printf("XML results\n");
    printf("--------------------------------------------------\n");
    print_results_xml(&results);
    printf("--------------------------------------------------\n");
    printf("TSV results\n");
    printf("--------------------------------------------------\n");
    print_results_tsv(&results);
    printf("--------------------------------------------------\n");

}

