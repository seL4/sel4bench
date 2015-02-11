/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <assert.h>
#include <vka/vka.h>
#include <stdbool.h>
#include <simple/simple.h>
#include <sel4utils/mapping.h>
#include <sel4utils/util.h>

/*
 * Various helper functions for the seL4 test suite. See helpers.c for more
 * comments.
 */

#define IS_POWER_OF_2_OR_ZERO(x) (0 == ((x) & ((x) - 1)))
#define IS_POWER_OF_2(x) (((x) != 0) && IS_POWER_OF_2_OR_ZERO(x))
#define ALIGN_UP(x, n) (((x) + (n) - 1) & ~((n) - 1))
#define ALIGN_DOWN(x, n) ((x) & ~((n) - 1))

/* Stringify a macro. use _STR; _FAKE_STR is an implementation artifact. */
#define _FAKE_STR(x) #x
#define _STR(x) _FAKE_STR(x)

/* Compute the length of an array. */
#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

/* Scheduler priorities. */
#define OUR_PRIO 255 /* Root task priority, and test thread priority. */

#define MIN_PRIO 0
#define MAX_PRIO (OUR_PRIO - 1)
#define NUM_PRIOS (MAX_PRIO - MIN_PRIO + 1)

int
__attribute__((warn_unused_result))
check_zeroes(seL4_Word addr, seL4_Word size_bytes);

int
is_slot_empty(seL4_Word slot);

int
are_tcbs_distinct(seL4_CPtr tcb1, seL4_CPtr tcb2);

seL4_CPtr
alloc_tcb(vka_t *vka);

seL4_Word
get_free_slot(vka_t *vka);

/* Four arguments ought to be enough for anyone. */
typedef seL4_Word (*helper_func_t)(
    seL4_Word, seL4_Word, seL4_Word, seL4_Word);

#define HELPER_THREAD_MAGIC 0x1337C0FF

struct helper_thread_desc {
    seL4_CPtr cspace;
    seL4_CPtr pd;

    helper_func_t func;
    seL4_Word flags;
    seL4_Word arg0;
    seL4_Word arg1;
    seL4_Word arg2;
    seL4_Word arg3;

    char use_remote_addr;
    seL4_CPtr remote_src_cspace;
    seL4_CPtr remote_ep_addr;
    seL4_CPtr remote_ep_cap;
    seL4_CPtr remote_fault_ep_addr;
};

typedef struct helper_thread_desc helper_thread_desc_t;

/*
 * This really should be a private struct, but we need our callers to
 * allocate space for it on their stacks, so they need to know what's in it.
 * Please treat this as otherwise private.
 */
struct helper_thread_data {
    /* This field is always HELPER_THREAD_MAGIC. */
    seL4_Word id_magic;

    /* This field is set to next_thread_magic_token and is unique for each
     * thread. */
    seL4_Word per_thread_magic;

    char name[12];

    helper_func_t func;
    seL4_Word arg0;
    seL4_Word arg1;
    seL4_Word arg2;
    seL4_Word arg3;

    seL4_CPtr tcb;
    seL4_CPtr local_endpoint;
    seL4_CPtr remote_endpoint;
    seL4_CPtr ipc_buffer_frame;
    seL4_Word ipc_buffer_vaddr;
    seL4_CPtr fault_ep;
    seL4_CPtr cspace;
    seL4_Word sp; /* Starting stack pointer. */
    seL4_Word flags;
    void* stack;
    seL4_CPtr stack_frame;

    vka_t *vka;

    /* This alignment is necessary so that we can fit the address of a helper
     * thread into a 28-bit badge. */
} __attribute__((aligned(16)));

typedef struct helper_thread_data helper_thread_t;

void
map_thread_ipc_buffer(vka_t *vka,
        seL4_CPtr pd, seL4_CPtr tcb,
        seL4_CPtr *ipc_buffer_frame, seL4_Word *ipc_buffer_vaddr);

void
free_thread_ipc_buffer(vka_t *vka,
        seL4_CPtr ipc_buffer_frame, seL4_Word ipc_buffer_vaddr);

seL4_Word
run_function_in_thread(vka_t *vka,
        helper_func_t func,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);

void
create_helper_thread_in_cspace_common(helper_thread_t *data, vka_t *vka,
        helper_thread_desc_t desc);

void
create_helper_thread(helper_thread_t *thread_data,
        vka_t *vka,
        helper_func_t func, seL4_Word flags,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);

void
create_helper_thread_in(helper_thread_t *data,
        vka_t *vka, seL4_CPtr cspace, seL4_CPtr pd,
        helper_func_t func, seL4_Word flags,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);

void
create_helper_thread_in_cspace(helper_thread_t *data, vka_t *vka,
        helper_thread_desc_t desc);

void
set_helper_thread_name(helper_thread_t *data, const char *name, ...);

void
start_helper_thread(helper_thread_t *data);

void
set_helper_thread_priority(helper_thread_t *data, uint8_t prio);

seL4_Word
wait_for_thread(helper_thread_t *data);

void
cleanup_helper_thread(helper_thread_t *data);

void *
run_on_stack(vka_t *vka, seL4_Word stack_size_bits,
             void * (*func)(void*), void*arg);

void
map_frame(vka_t *vka, seL4_CPtr frame, seL4_Word vaddr,
          seL4_CapRights rights, bool cacheable);

seL4_CPtr
create_vspace(vka_t *vka, simple_t *simple, seL4_CPtr cspace);
seL4_CPtr
create_cspace(vka_t *vka, simple_t *simple);

/* Fault handler thread, for use by main(). */
extern seL4_CPtr global_fault_handler_ep;
void
fault_handler_thread(seL4_CPtr fault_ep);

void
reset_helper_thread_desc(helper_thread_desc_t *desc);

#ifdef ARCH_ARM
#define seL4_4KPage seL4_ARM_SmallPageObject
#elif defined(ARCH_IA32)
#ifdef CONFIG_X86_64
#define seL4_4KPage seL4_X64_4K
#else
#define seL4_4KPage seL4_IA32_4K
#endif
#else
#error Unsupported architecture
#endif

#define ROOTTASK_FAULT_BADGE 0x7654321

#endif /* __HELPERS_H__ */
