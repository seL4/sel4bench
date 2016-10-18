/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#include <autoconf.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sel4bench/arch/sel4bench.h>

#include <benchmark.h>
#include <vka/capops.h>
#include <vka/vka.h>

#include "crypto.h"
#include "rijndael-alg-fst.h"

#include <aes.h>

typedef struct {
    uint8_t *vector;
    uint8_t *pt;
    uint8_t *ct;
    size_t len;
} state_t;

#define N_CLIENTS 10

static sel4utils_checkpoint_t cp;
static aes_results_t *results = NULL;

/* server state */
static volatile state_t *st = NULL;
static uint8_t dummy_pt[4096 * 100];
static uint8_t dummy_ct[sizeof(dummy_pt)];
static uint8_t dummy_iv[] = {0};
static uint32_t rk[4 * (AES256_KEY_ROUNDS + 1)];


static sel4utils_thread_t server_thread;
static sel4utils_thread_t clients[N_CLIENTS];
static cspacepath_t slot;
static seL4_CPtr done_ep;
static seL4_CPtr init_ep;

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

void server_fn(seL4_CPtr ep)
{
    state_t s2, s;
    st = NULL;

    /* init */
    uint8_t key[AES256_KEY_BITS / 8];
    for (unsigned i = 0; i < sizeof key; i++)
             key[i] = i;



    ZF_LOGV("Server started\n");
    seL4_SignalRecv(init_ep, ep, NULL);

    for (int req = 0; true; req++) {
        assert(st == NULL);

        // XXX: Unpack state into `s`
        s.vector = (uint8_t *) seL4_GetMR(0);
        if (s.vector == NULL) {
            /* new request */
            s.pt = dummy_pt;
            s.ct = dummy_ct;
            s.len = sizeof(dummy_pt);
            s.vector = dummy_iv;
        } else {
            /* continued request */
            s.pt = (uint8_t *) seL4_GetMR(1);
            s.ct = (uint8_t *) seL4_GetMR(2);
            s.len = (size_t) seL4_GetMR(3);
        }

        st = &s;

        while (st->len > 0) {
            assert(st->len <= sizeof(dummy_pt));
            uint8_t pt_block[AES_BLOCK_SIZE];
            for (unsigned i = 0; i < AES_BLOCK_SIZE; i++) {
                pt_block[i] = st->pt[i] ^ st->vector[i];
            }

            rijndaelEncrypt(rk, AES256_KEY_ROUNDS, pt_block, st->ct);

            if (st == &s) {
            /* swap to s2 */
                s2.vector = st->ct;
                s2.pt = st->pt + AES_BLOCK_SIZE;
                s2.ct = st->ct + AES_BLOCK_SIZE;
                s2.len = st->len - AES_BLOCK_SIZE;
                SEL4BENCH_FENCE();
                st = &s2;
            } else {
                /* swap to s */
                s.vector = st->ct;
                s.pt = st->pt + AES_BLOCK_SIZE;
                s.ct = st->ct + AES_BLOCK_SIZE;
                s.len = st->len - AES_BLOCK_SIZE;
                SEL4BENCH_FENCE();
                st = &s;
            }
        }
        ZF_LOGV("Done ");
        seL4_SetMR(3, 0);
        seL4_ReplyRecv(ep, seL4_MessageInfo_new(0, 0, 0, 4), NULL);
        st = NULL;
    }

    /* server never exits */
}

void
client_fn(seL4_CPtr ep, seL4_CPtr unused)
{
    seL4_Word mr0, mr1, mr2, mr3;

    mr3 = 0;
    while (true) {
		if (mr3 != 0) {
            seL4_SetMR(0, mr0);
            seL4_SetMR(1, mr1);
            seL4_SetMR(2, mr2);
            seL4_SetMR(3, mr3);
        } else {
            seL4_SetMR(0, 0);
        }

        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 4));

        mr0 = seL4_GetMR(0);
        mr1 = seL4_GetMR(1);
        mr2 = seL4_GetMR(2);
        mr3 = seL4_GetMR(3);
    }
}

static inline void
handle_timeout_murder(seL4_CPtr ep)
{
    /* take reply cap */
    int error = vka_cnode_swapTCBCaller(&slot, &server_thread.tcb);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to get servers reply cap");
    /* kill the reply cap  - this will return the client sc along the call chain */
    error = vka_cnode_delete(&slot);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to delete reply cap");

    //give server time to get back on ep (or could extend client budget)
    error = seL4_SchedContext_Bind(server_thread.sched_context.cptr, server_thread.tcb.cptr);
    ZF_LOGF_IF(error != 0, "Failed to bind sc to server");

    ZF_LOGD("Killed client\n");
    /* restore server */
    st = NULL;
    sel4utils_checkpoint_restore(&cp, false, true);

    ZF_LOGD("Waiting for server to reply\n");
    // wait for server to init and take context back
    seL4_Recv(init_ep, NULL);

    /* convert server back to passive */
    error = seL4_SchedContext_Unbind(server_thread.sched_context.cptr);
    ZF_LOGF_IF(error != 0, "Failed to unbind sc from server");
}

void restart_clients(void)
{
    /* restart clients for more murder */
    for (int i = 0; i < N_CLIENTS; i++) {
        int error = seL4_TCB_Resume(clients[i].tcb.cptr);
        ZF_LOGF_IF(error != seL4_NoError, "Failed to restart client");
    }
}

static inline void
handle_timeout_extend(seL4_CPtr sched_ctrl, seL4_Word data, uint64_t more, uint64_t period)
{
    /* give the client more budget */
    ZF_LOGD("Got b %llu, p %llu, %d\n", more, period, data);
    int error = seL4_SchedControl_Configure(sched_ctrl, clients[data].sched_context.cptr, more, period, data);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to extend budget");
}

static inline void
handle_timeout_emergency_budget(seL4_CPtr ep, seL4_Word data)
{
    /* for this timeout fault handler, we give the server some extra budget,
     * we run at higher prio to the server to we call into its queue and take the budget
     * away once it responds
     */

    /* take away clients sc from the server */
    int error = seL4_SchedContext_UnbindObject(clients[data].sched_context.cptr, server_thread.tcb.cptr);
    ZF_LOGF_IF(error != seL4_NoError, "failed to unbind client sc from server");

    /* bind server with emergency budget */
    error = seL4_SchedContext_Bind(server_thread.sched_context.cptr, server_thread.tcb.cptr);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to bind server emergency budget");

    ZF_LOGV("Call server\n");
    seL4_Reply(seL4_MessageInfo_new(0, 0, 0, 0));
}

static inline void
handle_timeout_rollback(seL4_CPtr ep)
{
    /* restore client */
    UNUSED int error = vka_cnode_swapTCBCaller(&slot, &server_thread.tcb);
    assert(error == 0);
    if (st != NULL) {
        ZF_LOGV("Restored client %p %p %p %u\n", st->vector, st->pt, st->ct, st->len);
        seL4_SetMR(0, (seL4_Word) st->vector);
        seL4_SetMR(1, (seL4_Word) st->pt);
        seL4_SetMR(2, (seL4_Word) st->ct);
        seL4_SetMR(3, (seL4_Word) st->len);
    } else {
        ZF_LOGV("Failed\n");
        seL4_SetMR(3, sizeof(dummy_pt));
    }
    /* reply to client */
    seL4_Send(slot.capPtr, seL4_MessageInfo_new(0, 0, 0, 4));

    st = NULL;
    sel4utils_checkpoint_restore(&cp, false, true);
    //give server time to get back on ep (or could extend client budget)
    error = seL4_SchedContext_Bind(server_thread.sched_context.cptr, server_thread.tcb.cptr);
    assert(error == 0);

     // wait for server to init and take context back
    seL4_Recv(init_ep, NULL);

    /* convert server back to passive */
    error = seL4_SchedContext_Unbind(server_thread.sched_context.cptr);
    assert(error == 0);
}

void
tfep_fn_emergency_budget(seL4_CPtr tfep, seL4_CPtr ep)
{
    seL4_Word badge;
    ccnt_t start, end;
     for (int i = 0; i < N_RUNS; i++) {
        seL4_Recv(tfep, &badge);
        ZF_LOGV("Fault from %d\n", seL4_GetMR(0));
        SEL4BENCH_READ_CCNT(start);
        handle_timeout_emergency_budget(ep, seL4_GetMR(seL4_TimeoutFault_Data));
        /* call the server with a fake finished request */
        seL4_SetMR(0, 0xdeadbeef);
        seL4_SetMR(3, 0);
        SEL4BENCH_READ_CCNT(end);
        results->emergency_cost[i] = end - start;
        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 4));
        /* server is done! remove emergency budget */
        seL4_SchedContext_Unbind(server_thread.sched_context.cptr);
     }

    /* cold cache */
    for (int i = 0; i < N_RUNS; i++) {
        seL4_Recv(tfep, &badge);
        ZF_LOGV("Fault from %d\n", seL4_GetMR(0));
        seL4_BenchmarkFlushCaches();
        SEL4BENCH_READ_CCNT(start);
        handle_timeout_emergency_budget(ep, seL4_GetMR(seL4_TimeoutFault_Data));
        seL4_SetMR(0, 0xdeadbeef);
        seL4_SetMR(3, 0);
        SEL4BENCH_READ_CCNT(end);
        results->emergency_cost_cold[i] = end - start;
        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 4));
        /* server is done! remove emergency budget */
        seL4_SchedContext_Unbind(server_thread.sched_context.cptr);
    }

    /* finished */
	seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    seL4_Recv(done_ep, NULL);
}

/* timeout fault handler for doing rollbacks */
void
tfep_fn_rollback(seL4_CPtr tfep, seL4_CPtr ep)
{
    ZF_LOGV("TFE started\n");
    ccnt_t start, end;
    seL4_Word badge;

    /* hot cache */
    for (int i = 0; i < N_RUNS; i++) {
        seL4_Recv(tfep, &badge);
        ZF_LOGV("Fault from %d\n", seL4_GetMR(0));
        SEL4BENCH_READ_CCNT(start);
        handle_timeout_rollback(ep);
        SEL4BENCH_READ_CCNT(end);
        results->rollback_cost[i] = end - start;
    }

    /* cold cache */
    for (int i = 0; i < N_RUNS; i++) {
        seL4_Recv(tfep, &badge);
        ZF_LOGV("Fault from %d\n", seL4_GetMR(0));
        seL4_BenchmarkFlushCaches();
        SEL4BENCH_READ_CCNT(start);
        handle_timeout_rollback(ep);
        SEL4BENCH_READ_CCNT(end);
        results->rollback_cost_cold[i] = end - start;
        ZF_LOGV("Recorded cost "CCNT_FORMAT, results->rollback_cost_cold[i]);
    }

    /* finished */
	seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    seL4_Recv(done_ep, NULL);
}

void
reset_budgets(seL4_CPtr sched_ctrl, uint64_t budgets[N_CLIENTS])
{
    for (int i = 0; i < N_CLIENTS; i++) {
        budgets[i] = 1 * US_IN_MS;
        ZF_LOGD("Set data to %d\n", i);
        int error = seL4_SchedControl_Configure(sched_ctrl, clients[i].sched_context.cptr, budgets[i],
                100 * US_IN_MS, i);
        ZF_LOGF_IF(error != seL4_NoError, "Failed to configure sc");
    }
}

/* timeout fault handler for doing rollbacks */
void
tfep_fn_extend(seL4_CPtr tfep, seL4_CPtr sched_ctrl)
{
    ZF_LOGV("TFE started\n");
    ccnt_t start, end;
    seL4_Word badge;
    uint64_t budgets[N_CLIENTS] = {0};

    seL4_Recv(tfep, &badge);
    seL4_Word data = seL4_GetMR(seL4_TimeoutFault_Data);
    /* run this N_CLIENTSx to get the results we want */
    for (int j = 0; j < N_CLIENTS + 1; j++) {
        /* set the clients budgets to be small */
        /* hot cache */
        for (int i = 0; i < N_CLIENTS; i++) {
            ZF_LOGV("Fault from %d\n", data);
            assert(data < N_CLIENTS);
            budgets[data] += US_IN_MS;
            SEL4BENCH_READ_CCNT(start);
            handle_timeout_extend(sched_ctrl, data, budgets[data], 100 * US_IN_MS);
            SEL4BENCH_READ_CCNT(end);
            results->extend_cost[j * N_CLIENTS + i] = end - start;
            seL4_ReplyRecv(tfep, seL4_MessageInfo_new(0, 0, 0, 0), &badge);
            data = seL4_GetMR(seL4_TimeoutFault_Data);
        }

        /* cold cache */
        reset_budgets(sched_ctrl, budgets);
        for (int i = 0; i < N_CLIENTS; i++) {
                ZF_LOGV("Fault from %d\n", data);
                budgets[data] += US_IN_MS;
                seL4_BenchmarkFlushCaches();
                SEL4BENCH_READ_CCNT(start);
                handle_timeout_extend(sched_ctrl, data, budgets[data], 100 * US_IN_MS);
                SEL4BENCH_READ_CCNT(end);
                results->extend_cost_cold[j * N_CLIENTS + i] = end - start;
                seL4_ReplyRecv(tfep, seL4_MessageInfo_new(0, 0, 0, 0), &badge);
                data = seL4_GetMR(seL4_TimeoutFault_Data);
        }
       reset_budgets(sched_ctrl, budgets);
    }

    /* finished */
	seL4_Send(done_ep, seL4_MessageInfo_new(0, 0, 0, 0));
    seL4_Recv(done_ep, NULL);
}

void
measure_ccnt_overhead(ccnt_t *results)
{
    ccnt_t start, end;
    for (int i = 0; i < N_RUNS; i++) {
        SEL4BENCH_READ_CCNT(start);
        SEL4BENCH_READ_CCNT(end);
        results[i] = (end - start);
    }
}

int
main(int argc, char **argv)
{
    UNUSED int error;
    vka_object_t ep, tfep;

    sel4bench_init();
    env_t *env = benchmark_get_env(argc, argv, sizeof(aes_results_t));
    results = (aes_results_t *) env->results;

    measure_ccnt_overhead(results->overhead);
    /* allocate an ep */
    error = vka_alloc_endpoint(&env->vka, &ep);
    ZF_LOGF_IF(error != 0, "Failed to allocate ep");

    vka_object_t init_epo;
    error = vka_alloc_endpoint(&env->vka, &init_epo);
    ZF_LOGF_IF(error != 0, "Failed to allocate ep");
    init_ep = init_epo.cptr;

    /* allocate a tfep */
    error = vka_alloc_endpoint(&env->vka, &tfep);
    ZF_LOGF_IF(error != 0, "Failed to allocate tfep");

    /* start the server */
    benchmark_configure_thread(env, 0, seL4_MaxPrio - 1, "server", &server_thread);
    //set servers tfep
     seL4_CapData_t guard = seL4_CapData_Guard_new(0, seL4_WordBits - CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    error = seL4_TCB_SetSpace(server_thread.tcb.cptr, seL4_CapNull, tfep.cptr,
        SEL4UTILS_CNODE_SLOT, guard, SEL4UTILS_PD_SLOT,
        seL4_CapData_Guard_new(0, 0));

    error = sel4utils_start_thread(&server_thread, server_fn, (void *) ep.cptr, NULL, true);
    ZF_LOGF_IF(error != 0, "Failed to start server");

    /* wait for server to init and convert to passive */
    seL4_Wait(init_ep, NULL);
    error = seL4_SchedContext_Unbind(server_thread.sched_context.cptr);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to convert server to passive");

    /* checkpoint server */
    sel4utils_checkpoint_thread(&server_thread, &cp, false);
    /* allocate a slot for tfep handler to use */
    seL4_CPtr tf_slot;
    error = vka_cspace_alloc(&env->vka, &tf_slot);
    ZF_LOGF_IF(error != 0, "Failed to allocate cslot for tfep");
    vka_cspace_make_path(&env->vka, tf_slot, &slot);

    /* start the temporal fault handler */
    sel4utils_thread_t tfep_thread;
    benchmark_configure_thread(env, 0, seL4_MaxPrio, "tfep", &tfep_thread);
    error = sel4utils_start_thread(&tfep_thread, tfep_fn_rollback, (void *) tfep.cptr, (void *) ep.cptr, true);
    ZF_LOGF_IF(error != 0, "Failed to start tfep thread");

    /* let it initialise */
    seL4_SchedContext_YieldTo_t ytr = seL4_SchedContext_YieldTo(tfep_thread.sched_context.cptr);
    ZF_LOGF_IF(ytr.error != seL4_NoError, "Failed to yield to tfep thread");

    /* create an ep for clients to signal on when they are done */
    vka_object_t done;
    error = vka_alloc_endpoint(&env->vka, &done);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate ep");

    /* start a client */
    benchmark_configure_thread(env, 0, seL4_MaxPrio - 2, "nice client", &clients[0]);
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), clients[0].sched_context.cptr, 1 * US_IN_MS, 10 * US_IN_MS, 0);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to configure sc");
    error = sel4utils_start_thread(&clients[0], client_fn, (void *) ep.cptr, (void *) done.cptr, true);
    ZF_LOGF_IF(error != 0, "Failed to start nice client");

    /* start another client* */
    benchmark_configure_thread(env, 0, seL4_MaxPrio - 2, "evil client", &clients[1]);
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), clients[1].sched_context.cptr, 5 * US_IN_MS, 10 * US_IN_MS, 1);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to configure sc");
    error = sel4utils_start_thread(&clients[1], client_fn, (void *) ep.cptr, (void *) done.cptr, true);
    ZF_LOGF_IF(error != 0, "Failed to start evil client");

    done_ep = done.cptr;

    /* wait for timeout fault handler to finish - it will exit once it has enough samples */
    benchmark_wait_children(done.cptr, "tfep", 1);

    /* next benchmark - use emergency sc's instead */
    ZF_LOGV("Starting emergency budget benchmark\n");
    /* reconfigure tfep and restart */
    error = sel4utils_start_thread(&tfep_thread, tfep_fn_emergency_budget, (void *) tfep.cptr, (void *) ep.cptr, true);
    ZF_LOGF_IF(error != seL4_NoError, "Failed to restart tfep");

    benchmark_wait_children(done.cptr, "tfep", 1);

    /* next benchmark - extend budget - we need more clients for this one, so set the rest up */
    seL4_TCB_Suspend(clients[0].tcb.cptr);
    seL4_TCB_Suspend(clients[1].tcb.cptr);
    for (int i = 0; i < N_CLIENTS; i++) {
        benchmark_configure_thread(env, 0, seL4_MaxPrio - 2, "client", &clients[i]);

        error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), clients[i].sched_context.cptr, 1 * US_IN_MS, 10 * US_IN_MS, i);
        ZF_LOGF_IF(error != seL4_NoError, "Failed to configure sc");
        error = sel4utils_start_thread(&clients[i], client_fn, (void *) ep.cptr, (void *) done.cptr, true);
        ZF_LOGF_IF(error != seL4_NoError, "failed to start client %d\n", i);
    }

    /* start the tfep */
    ZF_LOGV("Running extend benchmark");
    error = sel4utils_start_thread(&tfep_thread, tfep_fn_extend, (void *) tfep.cptr, (void *)
                simple_get_sched_ctrl(&env->simple), true);
    ZF_LOGF_IF(error != seL4_NoError, "failed to restart tfep");

    benchmark_wait_children(done.cptr, "tfep-extend", 1);

    /* done -> results are stored in shared memory so we can now return */
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}

