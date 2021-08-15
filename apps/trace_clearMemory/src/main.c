/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <autoconf.h>
#include <sel4benchtraceclearmemory/gen_config.h>

#include <stdio.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>
#include <vka/object.h>
#include <vka/capops.h>

#include <sel4bench/kernel_logging.h>
#include <sel4bench/logging.h>

#include <benchmark.h>
#include <trace_clearMemory.h>

#define N_RETYPES 100
#define MIB_PAGE_SIZE_BITS 20
void abort(void)
{
    benchmark_finished(EXIT_FAILURE);
}

static env_t *env;
static void*log_buffer;
void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {0};

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(trace_clear_memory_results_t),
              object_freq
          );
}

seL4_Word get_free_slot(vka_t *vka)
{
    seL4_CPtr slot;
    UNUSED int error = vka_cspace_alloc(vka, &slot);
    assert(!error);
    return slot;
}

static int bench_setup_log_buffer_frame(env_t *env)
{
    int err;

    seL4_CPtr frame = vka_alloc_frame_leaky(&env->delegate_vka, MIB_PAGE_SIZE_BITS);
    ZF_LOGF_IF(frame == seL4_CapNull, "fail");

    err = kernel_logging_set_log_buffer(frame);
    if (err != 0) {
        ZF_LOGE("Failed to set log buffer frame using cap %lu.\n",
                frame);

        return err;
    }

    log_buffer = vspace_map_pages(&env->vspace,
                                          &frame, NULL,
                                          seL4_AllRights, 1, MIB_PAGE_SIZE_BITS,
                                          0);
    if (log_buffer == NULL) {
        ZF_LOGE("Failed to map log buffer into VMM addrspace.\n");
        return -1;
    }

    memset((void *)log_buffer, 0, BIT(MIB_PAGE_SIZE_BITS));
    ZF_LOGE("Log buffer (cptr %lu) mapped to vaddr %p.\n",
            frame, log_buffer);
    return 0;
}

int main(int argc, char **argv)
{
    trace_clear_memory_results_t *results;
    printf("thing\n");
    results = (trace_clear_memory_results_t *) env->results;
    int error;

    sel4bench_init();
    seL4_CPtr frame, frame2, untyped;
    int err = bench_setup_log_buffer_frame(env);
    if (err != 0) {
        ZF_LOGE("Failed to setup log buffer for timestamps. Err %d.\n", err);
        return err;
    }

    untyped = vka_alloc_untyped_leaky(&env->slab_vka, PAGE_BITS_4K);
    ZF_LOGF_IF(untyped == seL4_CapNull, "fail");
    cspacepath_t src_path;
    vka_cspace_make_path(&env->slab_vka, untyped, &src_path);

    frame = get_free_slot(&env->slab_vka);
    ZF_LOGF_IF(frame == seL4_CapNull, "fail");
    cspacepath_t dest_path;
    vka_cspace_make_path(&env->slab_vka, frame, &dest_path);

    /* Record tracepoints for untypeRetype path */
    kernel_logging_reset_log();
    for (int i = 0; i < N_RETYPES; ++i) {
        error = seL4_Untyped_Retype(untyped, seL4_ARCH_4KPage, PAGE_BITS_4K, dest_path.root, dest_path.dest,
                            dest_path.destDepth, dest_path.offset, 1);
        /* Revoke the untyped. This deletes the frame. */
        vka_cnode_revoke(&src_path);

    }
    results->n = seL4_BenchmarkFinalizeLog();
    memcpy(results->kernel_log, log_buffer, results->n * sizeof(benchmark_tracepoint_log_entry_t));

    sel4bench_destroy();
    benchmark_finished(EXIT_SUCCESS);
    return 0;
}
