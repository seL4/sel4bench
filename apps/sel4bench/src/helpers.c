/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sel4/sel4.h>
#include <sel4/messages.h>
#include <twinkle/vka.h>
#include <vka/object.h>
#include <sel4utils/mapping.h>
#include <sel4platsupport/platsupport.h>
#include <simple/simple_helpers.h>
#include "mem.h"
#include "helpers.h"
#include <utils/util.h>

#if 0
#define D(x...) printf(x)
#else
#define D(x...)
#endif

/*
 * This is a unique number we expect to receive from a thread to know that
 * we are talking to the right one.
 */
static int next_thread_magic_token = 0xaaaabbbb;

/*
 * Check whether a given region of memory is zeroed out.
 */
int
check_zeroes(seL4_Word addr, seL4_Word size_bytes)
{
    assert(IS_ALIGNED(addr, sizeof(seL4_Word)));
    assert(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        if (*p++ != 0) {
            D("Found non-zero at position %d: %d\n", ((int)p) - (addr), p[-1]);
            return 0;
        }
    }
    return 1;
}

/* Determine whether a given slot in the init thread's CSpace is empty by
 * examining the error when moving a slot onto itself.
 *
 * Serves as == 0 comparator for caps.
 */
int
is_slot_empty(seL4_Word slot)
{
    int error;

    error = seL4_CNode_Move(seL4_CapInitThreadCNode, slot, seL4_WordBits,
            seL4_CapInitThreadCNode, slot, seL4_WordBits);
    assert(error == seL4_DeleteFirst || error == seL4_FailedLookup);

    return (error == seL4_FailedLookup);
}

/* Determine if two TCBs in the init thread's CSpace are not equal. Note that we
 * assume the thread is not currently executing.
 *
 * Serves as != comparator for caps.
 */
int
are_tcbs_distinct(seL4_CPtr tcb1, seL4_CPtr tcb2)
{
    int error, i;
    seL4_UserContext regs;

    /* Initialise regs to prevent compiler warning. */
    error = seL4_TCB_ReadRegisters(tcb1, 0, 0, 1, &regs);
    if (error) {
        return error;
    }

    for (i = 0; i < 2; ++i) {
#if defined(ARCH_IA32)
#ifdef CONFIG_X86_64
        regs.rip = i;
#else
        regs.eip = i;
#endif
#elif defined(ARCH_ARM)
        regs.pc = i;
#else
#error
#endif
        error = seL4_TCB_WriteRegisters(tcb1, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return error;
        }

        error = seL4_TCB_ReadRegisters(tcb2, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return error;
#if defined(ARCH_IA32)
#ifdef CONFIG_X86_64
        } else if (regs.rip != i) {
#else
        } else if (regs.eip != i) {
#endif
#elif defined(ARCH_ARM)
        } else if (regs.pc != i) {
#else
#error
#endif
            return 1;
        }
    }
    return 0;
}



/* Get a free slot. */
seL4_Word
get_free_slot(vka_t *vka)
{
    seL4_CPtr slot;
    int error = vka_cspace_alloc(vka, &slot);
    (void)error;
    assert(!error);
    return slot;
}

/*
 * Run a function on a larger stack.
 * stack_size_bits defines the size of the target stack and should be at least
 * 4k (12).
 */
int run_on_stack(vka_t *vka, seL4_Word stack_size_bits,
                 int (*func)(void))
{
    assert(stack_size_bits >= 12);
    assert(stack_size_bits < 32);
    seL4_Word stack_size = 1U << stack_size_bits;;

    /* Allocate a stack in memory. Add twice as much vspace for a guard page. */
    seL4_Word new_stack = vmem_alloc(stack_size * 2, 4096);
    seL4_Word stack_start = new_stack + stack_size;
    for (int i = 0; i < stack_size / 4096; i++) {
        seL4_CPtr frame = vka_alloc_frame_leaky(vka, seL4_PageBits);
        sel4utils_map_page_leaky(vka, seL4_CapInitThreadPD, frame, (void*)stack_start + (i * 4096), seL4_AllRights, true);
    }

    return utils_run_on_stack((void *) stack_start + stack_size, func);
}


/*
 * Allocates a frame for an IPC buffer and sets it up for the given thread.
 * The frame cap and virtual address are returned by reference.
 * When done, you should call free_thread_ipc_buffer() with these returned
 * values to clean up.
 */
void
map_thread_ipc_buffer(vka_t *vka, seL4_CPtr pd, seL4_CPtr tcb,
        seL4_CPtr *ipc_buffer_frame, seL4_Word *ipc_buffer_vaddr)
{
    int error;
    (void)error;

    D("Creating an IPC buffer... ");
    seL4_CPtr buffer = vka_alloc_frame_leaky(vka, seL4_PageBits);
    *ipc_buffer_frame = buffer;
    D(" created at 0x%x.\n", (int)buffer);

    /*
     * Map in the frame. Map the 4K frame into the middle of a 16K region, so
     * that we can catch some out-of-bounds errors.
     */
    seL4_Word vaddr = vmem_alloc(16384, 4096);
    assert(vaddr != 0);
    seL4_Word mapped_vaddr = vaddr + 8192;
    *ipc_buffer_vaddr = mapped_vaddr;
    D("Mapping 4K frame (0x%x) to vaddr %p.\n",
      buffer, (void*)mapped_vaddr);
    sel4utils_map_page_leaky(vka, pd, buffer, (void*)mapped_vaddr, seL4_AllRights, true);

    /* Setup the IPC buffer for the new thread. */
    D("Setting up IPC buffer on thread %x...\n", tcb);
    error = seL4_TCB_SetIPCBuffer(tcb, mapped_vaddr, buffer);
    assert(!error);
}

void
free_thread_ipc_buffer(vka_t *vka,
        seL4_CPtr ipc_buffer_frame, seL4_Word ipc_buffer_vaddr)
{
    int error;
    (void) error;

    vmem_free(ipc_buffer_vaddr - 8192);

    /* Deleting the frame cap will implicitly unmap it too. */
    error = seL4_CNode_Delete(seL4_CapInitThreadCNode,
            ipc_buffer_frame, seL4_WordBits);
    assert(!error);
}

void
unmap_frame(seL4_CPtr frame)
{
    int error = seL4_ARCH_Page_Unmap(frame);
    (void)error;
    assert(!error);
}

void platsupport_zero_globals(void);

static volatile int serial_init_lock = 2;

/* Internal function used for all helper threads. */
static void
helper_thread(seL4_Word ep)
{
    seL4_Word *p;
    helper_thread_t _data;
    helper_thread_t *data = &_data;
    seL4_Word sender_badge = 0;
    seL4_MessageInfo_t tag;

    while (serial_init_lock == 0) {
        if (__sync_bool_compare_and_swap(&serial_init_lock, 0, 1)) {
            platsupport_zero_globals();
            platsupport_serial_setup_bootinfo_failsafe();
            serial_init_lock = 2;
        }
    }
    while (serial_init_lock == 1);

    D("Calling home on endpoint %d...\n", ep);
    /* Phone back to our creator to say we're alive. */
    tag = seL4_Wait(ep, &sender_badge);

    /* And they will tell us everything we need to know. */
    assert(seL4_MessageInfo_get_length(tag) == sizeof(*data) / sizeof(seL4_Word));
    p = (seL4_Word*)data;
    for (int i = 0; i < seL4_MessageInfo_get_length(tag); i++) {
        p[i] = seL4_GetMR(i);
    }

    /* Reply to say we're good! */
    seL4_MessageInfo_ptr_set_length(&tag, 0);
    seL4_Reply(tag);

    /* We are now good to go! */
    seL4_Word retval;
    D("Executing helper\n");
    retval = data->func(data->arg0, data->arg1, data->arg2, data->arg3);

    /*
     * Call back to our creator. If our creator was waiting for us, will
     * will probably die as soon as the message is received. Otherwise, we
     * will block indefinitely.
     */
    seL4_MessageInfo_ptr_set_length(&tag, 2);
    seL4_SetMR(0, data->per_thread_magic);
    seL4_SetMR(1, retval);
    D("Calling back\n");
    tag = seL4_Call(ep, tag);
    D("Callback returned... %p\n", (void*)tag.words[0]);

    /* Shouldn't get here. */
    assert(0);
    for (;;) {
        ;
    }
}

/* Create, but do not start, a helper thread running. */
void
create_helper_thread_in_cspace_common(helper_thread_t *data, vka_t *vka,
        helper_thread_desc_t desc)
{
    data->vka = vka;
    data->flags = desc.flags;
    data->cspace = desc.cspace;

    int error;
    (void)error;

    /* Allocate a stack for this thread.
     * Use the start of the stack to pass its data and arguments.
     * We allocate a 4K stack 56KB into a 64KB region and use the rest as a
     * guard.
     */
    D("Allocating thread stack... ");
    seL4_Word thread_stack_region = vmem_alloc(64 * 1024, 4096);
    assert(thread_stack_region != 0);
    D("created at %p ", (void*)thread_stack_region);

    seL4_Word thread_stack_va = thread_stack_region + 56 * 1024;
    seL4_CPtr thread_stack_frame = vka_alloc_frame_leaky(vka, seL4_PageBits);
    D("with frame %d\n", thread_stack_frame);
    sel4utils_map_page_leaky(vka, seL4_CapInitThreadPD, thread_stack_frame,
            (void*)thread_stack_va, seL4_AllRights, true);

    data->stack = (void*)thread_stack_region;
    data->stack_frame = thread_stack_frame;
    data->sp = thread_stack_va + 4096;

    seL4_Word thread_magic = next_thread_magic_token++;

    data->func = desc.func;
    data->arg0 = desc.arg0;
    data->arg1 = desc.arg1;
    data->arg2 = desc.arg2;
    data->arg3 = desc.arg3;
    data->per_thread_magic = thread_magic;

    /* Create a TCB. */
    D("Creating a TCB...");
    seL4_CPtr tcb = vka_alloc_tcb_leaky(vka);
    D(" created at 0x%x.\n", (int)tcb);
    data->tcb = tcb;

#ifdef ARCH_IA32
#ifdef CONFIG_X86_64
    data->sp -= 8;
    *(void **)data->sp = (void *)data->remote_endpoint;
    data->sp -= 8;
#else
    /* Push the recipient's endpoint onto the stack. */
    data->sp -= 4;
    *(void**)data->sp = (void*)data->remote_endpoint;

    /* And now push on a fake return address. */
    data->sp -= 4;
    *(void**)data->sp = 0;
#endif
#endif

    /* Unmap the stack and map it into the PD it needs to be in. */
    if (desc.pd != seL4_CapInitThreadPD) {
        unmap_frame(thread_stack_frame);
        sel4utils_map_page_leaky(vka, desc.pd,
                thread_stack_frame, (void*)thread_stack_va, seL4_AllRights, true);
    }

    /* Create an IPC ipc_buffer_frame. */
    map_thread_ipc_buffer(vka, desc.pd, tcb,
            &data->ipc_buffer_frame,
            &data->ipc_buffer_vaddr);

    /* Setup the VSpace / CSpace of the new thread. */
    D("Setting TCB CSpace and VSpace...\n");
    error = seL4_TCB_SetSpace(tcb,
            data->fault_ep,
            desc.cspace, seL4_NilData,
            desc.pd, seL4_NilData);
    assert(!error);

    data->id_magic = HELPER_THREAD_MAGIC;

    /* Set default priority as high as possible. */
    set_helper_thread_priority(data, OUR_PRIO - 1);
}

/* Creates a thread with local or remote cspace. */
void
create_helper_thread_in_cspace(helper_thread_t *data, vka_t *vka,
        helper_thread_desc_t desc)
{
    /* Clear out thread data. */
    memset(data, 0, sizeof(*data));

    int error;
    (void)error;
    if (desc.use_remote_addr) {
        /* Creating thread in remote cspace. */
        data->local_endpoint = desc.remote_ep_cap;
        data->remote_endpoint = desc.remote_ep_addr;
        error = seL4_CNode_Copy(
                    desc.cspace, data->remote_endpoint, seL4_WordBits,
                    desc.remote_src_cspace, data->local_endpoint, seL4_WordBits,
                    seL4_AllRights);
        assert(!error);
        data->fault_ep = desc.remote_fault_ep_addr;
    } else {
        /* Creating thread in local cspace. */
        D("Creating an Endpoint object...");
        seL4_CPtr endpoint = vka_alloc_endpoint_leaky(vka);
        D(" created at 0x%x.\n", (int)endpoint);
        data->local_endpoint = endpoint;
        data->remote_endpoint = endpoint;
        /* Copy into the remote cspace if need be. */
        if (desc.cspace != seL4_CapInitThreadCNode) {
            D("Performting a copy of endpoint into other cspace\n");
            error = seL4_CNode_Copy(
                        desc.cspace, data->remote_endpoint, seL4_WordBits,
                        seL4_CapInitThreadCNode, endpoint, seL4_WordBits,
                        seL4_AllRights);
            assert(!error);
        }
        data->fault_ep = get_free_slot(vka);
    }
    /* Create a badged fault endpoint for this thread. */
    D("Minting new fault endpoint at 0x%x\n", data->fault_ep);
    seL4_CapData_t cap_data = seL4_CapData_Badge_new(((seL4_Word)data) >> 4);
    error = seL4_CNode_Mint(
                desc.cspace, data->fault_ep, seL4_WordBits,
                seL4_CapInitThreadCNode, global_fault_handler_ep, seL4_WordBits,
                seL4_AllRights, cap_data);
    assert(!error);
    /* Create the thread. */
    create_helper_thread_in_cspace_common(data, vka, desc);
}

void
create_helper_thread_in(helper_thread_t *data,
        vka_t *vka, seL4_CPtr cspace, seL4_CPtr pd,
        helper_func_t func, seL4_Word flags,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    helper_thread_desc_t desc;
    reset_helper_thread_desc(&desc);
    desc.cspace = cspace;
    desc.pd = pd;
    desc.func = func;
    desc.flags = flags;
    desc.arg0 = arg0;
    desc.arg1 = arg1;
    desc.arg2 = arg2;
    desc.arg3 = arg3;
    desc.use_remote_addr = false;

    create_helper_thread_in_cspace(data, vka, desc);
}

void
create_helper_thread(helper_thread_t *data,
        vka_t *vka,
        helper_func_t func, seL4_Word flags,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    create_helper_thread_in(
        data, vka, seL4_CapInitThreadCNode, seL4_CapInitThreadPD,
        func, flags, arg0, arg1, arg2, arg3);
}

void
set_helper_thread_name(helper_thread_t *data, const char *name, ...)
{
    va_list ap;
    va_start(ap, name);
    vsnprintf(data->name, ARRAY_LEN(data->name), name, ap);
    va_end(ap);
    data->name[ARRAY_LEN(data->name) - 1] = '\0';
}

void
set_helper_thread_priority(helper_thread_t *data, uint8_t prio)
{
    int error = seL4_TCB_SetPriority(data->tcb, prio);
    (void)error;
    assert(!error);
}

/* Start a helper thread executing! */
void
start_helper_thread(helper_thread_t *data)
{
    /* Startup a new thread. */
    D("Starting up new thread...\n");
    seL4_UserContext frame = {
#ifdef ARCH_IA32
#ifdef CONFIG_X86_64
        .rdi = (unsigned long)data->remote_endpoint,
        .rip = (unsigned long)helper_thread,
        .rsp = (unsigned long)data->sp,
        .gs  = IPCBUF_GDT_SELECTOR
#else
        .eip = (unsigned int)helper_thread,
        .esp = (unsigned int)data->sp,
        .gs = IPCBUF_GDT_SELECTOR
#endif
#else
        .r0 = (unsigned int)data->remote_endpoint,
        .pc = (unsigned int)helper_thread,
        .sp = (unsigned int)data->sp
#endif
    };
    int error = seL4_TCB_WriteRegisters(data->tcb, true, 0,
                sizeof(seL4_UserContext) / sizeof(seL4_Word), &frame);
    (void)error;
    assert(!error);

    /* Wait for the thread to IPC us. */
    D("Sending info to helper thread...\n");
    seL4_MessageInfo_t tag;
    seL4_Word *p = (seL4_Word*)data;
    int i;
    for (i = 0; i < sizeof(*data) / sizeof(seL4_Word); i++) {
        seL4_SetMR(i, p[i]);
    }
    seL4_MessageInfo_ptr_new(&tag, 0, 0, 0, i);
    tag = seL4_Call(data->local_endpoint, tag);
    (void) tag;
    assert(seL4_MessageInfo_get_length(tag) == 0);

    /* Thread is go go go! */
}

/* Wait for a helper thread to finish executing, collecting its return code. */
seL4_Word
wait_for_thread(helper_thread_t *data)
{
    seL4_Word sender_badge = 0;
    seL4_MessageInfo_t tag;

    /* Wait for the thread to IPC us again. */
    D("Waiting for thread to finish.\n");

    tag = seL4_Wait(data->local_endpoint, &sender_badge);
    (void)tag;
    assert(seL4_MessageInfo_get_length(tag) == 2);
    assert(seL4_GetMR(0) == data->per_thread_magic);
    seL4_Word retval = seL4_GetMR(1);

    return retval;
}

/*
 * Free up resources associated with a helper thread. It should not be
 * running.
 */
void
cleanup_helper_thread(helper_thread_t *data)
{
    /* Clean up, thread first. */
    D("Cleaning up thread.\n");
    int error = seL4_CNode_Delete(seL4_CapInitThreadCNode,
                data->tcb, seL4_WordBits);
    (void)error;
    assert(!error);

    error = seL4_CNode_Revoke(seL4_CapInitThreadCNode,
            data->local_endpoint, seL4_WordBits);
    assert(!error);

    error = seL4_CNode_Delete(seL4_CapInitThreadCNode,
            data->local_endpoint, seL4_WordBits);
    assert(!error);

    D("Cleaning fault_ep at 0x%x\n", data->fault_ep);
    seL4_CNode_Delete(data->cspace, data->fault_ep, seL4_WordBits);

    free_thread_ipc_buffer(data->vka,
            data->ipc_buffer_frame, data->ipc_buffer_vaddr);

    vmem_free((seL4_Word)data->stack);

    error = seL4_CNode_Delete(seL4_CapInitThreadCNode,
            data->stack_frame, seL4_WordBits);
    assert(!error);
}

/*
 * Run a function in a separate thread in the same address space, and wait
 * for its completion.
 */
seL4_Word
run_function_in_thread(vka_t *vka,
        helper_func_t func,
        seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    struct helper_thread_data thread_data;

    create_helper_thread(&thread_data,
            vka, func, 0, arg0, arg1, arg2, arg3);

    start_helper_thread(&thread_data);

    seL4_Word retval = wait_for_thread(&thread_data);

    cleanup_helper_thread(&thread_data);

    return retval;
}

/*
 * Create a new vspace that is a distinct copy of our own.
 *
 * We currently expect cspace to be different from our own.
 */
seL4_CPtr
create_vspace(vka_t *vka, simple_t *simple, seL4_CPtr cspace)
{
    int error;
    seL4_CPtr pd;
    (void)error;
    assert(cspace != seL4_CapInitThreadCNode);

    /* Create a new page directory (vspace). */

#ifdef CONFIG_X86_64
    pd = vka_alloc_page_map_level4_leaky(vka);
    error = seL4_X64_PageMapLevel4_Map(pd);
#else
    pd = vka_alloc_page_directory_leaky(vka);
    /* Allocate it to the ASID pool. Need to do this in order to use it. */
    error = simple_ASIDPool_assign(simple, pd);
#endif
    assert(!error);
    /* Set the cspace's self vspace pointer. */
    seL4_CNode_Delete(cspace, seL4_CapInitThreadPD, seL4_WordBits);
    error = seL4_CNode_Copy(
                cspace, seL4_CapInitThreadPD, seL4_WordBits,
                seL4_CapInitThreadCNode, pd, seL4_WordBits,
                seL4_AllRights);
    assert(!error);

    /*
     * We deal with two regions.
     *    __executable_start -> _etext : read-only
     *    _etext -> __end__ : read-write
     * The read-only section just uses copies of the frame caps.
     * The read-write section deep copies the pages too.
     */
    extern char __executable_start[];
    extern char _etext[];
    extern char _end[];

    seL4_Word ro_start = (seL4_Word)__executable_start;
    seL4_Word rw_start = (seL4_Word)_etext;
    seL4_Word va_end = (seL4_Word)_end;
    /* Round rw_start into the ro section as we're going to copy those pages. */
    rw_start = ALIGN_DOWN(rw_start, 4096);
    va_end = ALIGN_UP(va_end, 4096);

    /*
     * Map everything from __executable_start to _etext (rounded down)
     * Assume that __executable_start begins in the second userland frame
     * (which should be true).
     */

    seL4_CPtr first_frame = simple_get_nth_userimage(simple, 0);
    int num_ro_frames = (rw_start - ro_start) / 4096;
    int num_rw_frames = (va_end - rw_start) / 4096;

    /* Double-check that we know what we're doing. */
    assert(num_ro_frames + num_rw_frames ==
           simple_get_userimage_count(simple));

    /* Now, map the read-only section, frame by frame. */
    seL4_Word va = ro_start;
    seL4_CPtr tmpslot = get_free_slot(vka);
    for (seL4_CPtr c = first_frame; c != first_frame + num_ro_frames; c++) {
        /* Grab their copy of the original frame cap. */
        error = seL4_CNode_Move(
                    seL4_CapInitThreadCNode, tmpslot, seL4_WordBits,
                    cspace, c, seL4_WordBits);
        assert(!error);

        /* Map it into their PD. */
        sel4utils_map_page_leaky(vka, pd,
                tmpslot, (void*)va, seL4_CanRead, true);

        /* Now move the cap into their cspace. */
        error = seL4_CNode_Move(
                    cspace, c, seL4_WordBits,
                    seL4_CapInitThreadCNode, tmpslot, seL4_WordBits);
        assert(!error);

        va += 4096;
    }

    /*
     * Map the read-write section, copying frames as we go.
     * To handle multiple threads in a vspace attempting to initialize
     * the serial we first initialize a lock into a state that
     * the helper threads can use to synchronize
     * this assumes multiple threads in a single vspace are not
     * spawning new threads concurrently. God this library is a fucking mess!
     */
    assert(serial_init_lock = 2);
    serial_init_lock = 0;

    /* This region will be used for copying data. */
    seL4_Word tmp_va = vmem_alloc(4096, 4096);
    assert(va == rw_start);

    for (seL4_CPtr c = first_frame + num_ro_frames;
            c != first_frame + num_ro_frames + num_rw_frames; c++) {
        /* Make a new frame for it. */
        seL4_CPtr frame = vka_alloc_frame_leaky(vka, seL4_PageBits);

        /* Map it in to our vspace so that we can copy data into it. */
        sel4utils_map_page_leaky(vka, seL4_CapInitThreadPD, frame, (void*)tmp_va, seL4_AllRights, true);
        memcpy((void*)tmp_va, (void*)va, 4096);
        unmap_frame(frame);

        /* Map it into their vspace. */
        sel4utils_map_page_leaky(vka, pd,
                frame, (void*)va, seL4_CanRead | seL4_CanWrite, true);

        /* Now move the cap into their cspace, deleting it first if need be. */
        seL4_CNode_Delete(cspace, c, seL4_WordBits);
        error = seL4_CNode_Move(
                    cspace, c, seL4_WordBits,
                    seL4_CapInitThreadCNode, frame, seL4_WordBits);
        assert(!error);

        va += 4096;
    }

    /* Set the serial lock back so if we spawn more threads in our own vspace
     * it is back to a sane state */
    serial_init_lock = 2;

    /* Copy the bootinfo frame, read-only. */
    {
        /* Grab their copy of the original frame cap. */
        error = seL4_CNode_Move(
                    seL4_CapInitThreadCNode, tmpslot, seL4_WordBits,
                    cspace, seL4_CapBootInfoFrame, seL4_WordBits);
        assert(!error);

        /* Map it into their PD. */
        sel4utils_map_page_leaky(vka, pd,
                tmpslot, (void*)seL4_GetBootInfo(), seL4_CanRead, true);

        /* Now move the cap back into their cspace. */
        error = seL4_CNode_Move(
                    cspace, seL4_CapBootInfoFrame, seL4_WordBits,
                    seL4_CapInitThreadCNode, tmpslot, seL4_WordBits);
        assert(!error);
    }

    vmem_free(tmp_va);

    return pd;
}

/*
 * Create a new cspace that looks vaguely like our own.
 */
seL4_CPtr
create_cspace(vka_t *vka, simple_t *simple)
{
    seL4_CPtr orig_cspace;
    seL4_CPtr cspace;
    int cnode_size;
    int error;
    (void)error;

    /* Create a CNode the same size as the root cnode. */
    cnode_size = simple_get_cnode_size_bits(simple);
    orig_cspace = vka_alloc_cnode_object_leaky(vka, cnode_size);

    /*
     * Set the actual depth of the CNode to 32. This involves moving the cap
     * into a new slot.
     */
    cspace = get_free_slot(vka);
    seL4_CapData_t cap_data;
    cap_data = seL4_CapData_Guard_new(0, (seL4_WordBits - cnode_size));
    error = seL4_CNode_Mutate(
                seL4_CapInitThreadCNode, cspace, seL4_WordBits,
                seL4_CapInitThreadCNode, orig_cspace, seL4_WordBits,
                cap_data);
    assert(!error);

    /* Now populate with the relevant caps. */

    /* Copy in cap to self cspace. */
    error = seL4_CNode_Copy(
                cspace, seL4_CapInitThreadCNode, seL4_WordBits,
                seL4_CapInitThreadCNode, cspace, seL4_WordBits,
                seL4_AllRights);
    assert(!error);
    D("Getting simple to copy all caps except the untyped caps\n");
    error = simple_copy_caps(simple, cspace, 0);
    D("Simple is done copying\n");
    assert(!error);

    return cspace;
}

static int
is_read_fault(void)
{
#if defined(ARCH_ARM)
    seL4_Word fsr = seL4_GetMR(SEL4_PFIPC_FSR);
#  if defined(ARM_HYP)
    return (fsr & (1 << 6)) == 0;
#  else
    return (fsr & (1 << 11)) == 0;
#  endif
#elif defined(ARCH_IA32)
    seL4_Word fsr = seL4_GetMR(SEL4_PFIPC_FSR);
    return (fsr & (1 << 1)) == 0;
#else
#error "Unknown architecture."
#endif
}

#define COLOR_ERROR "\033[1;31m"
#define COLOR_NORMAL "\033[0m"

void
fault_handler_thread(seL4_CPtr fault_ep)
{
    while (1) {
        helper_thread_t *thread;
        char tmpbuf[sizeof(thread->name)];
        const char *thread_name;

        seL4_MessageInfo_t tag;
        seL4_Word sender_badge = 0;

        tag = seL4_Wait(fault_ep, &sender_badge);

        if (sender_badge == 0) {
            thread_name = "unknown thread (badge 0)";
        } else if (sender_badge == ROOTTASK_FAULT_BADGE) {
            thread_name = "root task";
        } else {
            thread = (void*)(sender_badge << 4);

            /* If this assertion failed, we have no idea what the badge means. */
            if (thread->id_magic != HELPER_THREAD_MAGIC) {
                thread_name = "BROKEN THREAD";
            } else {
                if (thread->name[0] == '\0') {
                    snprintf(tmpbuf, ARRAY_LEN(tmpbuf), "tcb "DFMT"", thread->tcb);
                    thread_name = tmpbuf;
                } else {
                    thread_name = thread->name;
                }
            }
        }

        switch (seL4_MessageInfo_get_label(tag)) {
        case SEL4_PFIPC_LABEL:
            assert(seL4_MessageInfo_get_length(tag) == SEL4_PFIPC_LENGTH);
            printf("%sPagefault from [%s]: %s %s at PC: 0x"XFMT" vaddr: 0x"XFMT"%s\n",
                   COLOR_ERROR,
                   thread_name,
                   is_read_fault() ? "read" : "write",
                   seL4_GetMR(SEL4_PFIPC_PREFETCH_FAULT) ? "prefetch fault" : "fault",
                   seL4_GetMR(SEL4_PFIPC_FAULT_IP),
                   seL4_GetMR(SEL4_PFIPC_FAULT_ADDR),
                   COLOR_NORMAL);
            break;

        case SEL4_EXCEPT_IPC_LABEL:
            assert(seL4_MessageInfo_get_length(tag) == SEL4_EXCEPT_IPC_LENGTH);
            printf("%sBad syscall from [%s]: scno "DFMT" at PC: 0x"XFMT"%s\n",
                   COLOR_ERROR,
                   thread_name,
                   seL4_GetMR(EXCEPT_IPC_SYS_MR_SYSCALL),
#if defined(ARCH_ARM)
                   seL4_GetMR(EXCEPT_IPC_SYS_MR_PC),
#elif defined(ARCH_IA32)
#ifdef CONFIG_X86_64
                   seL4_GetMR(EXCEPT_IPC_SYS_MR_RIP),
#else
                   seL4_GetMR(EXCEPT_IPC_SYS_MR_EIP),
#endif
#else
#error "Unknown architecture."
#endif
                   COLOR_NORMAL
                  );

            break;

        case SEL4_USER_EXCEPTION_LABEL:
            assert(seL4_MessageInfo_get_length(tag) == SEL4_USER_EXCEPTION_LENGTH);
            printf("%sInvalid instruction from [%s] at PC: 0x"XFMT"%s\n",
                   COLOR_ERROR,
                   thread_name,
                   seL4_GetMR(0),
                   COLOR_NORMAL);
            break;

        default:
            /* What? Why are we here? What just happened? */
            printf("Unknown fault from [%s]: "DFMT" (length = "DFMT")\n", thread_name, seL4_MessageInfo_get_label(tag), seL4_MessageInfo_get_length(tag));
            assert(0);
            break;
        }
    }
}

void
reset_helper_thread_desc(helper_thread_desc_t *desc)
{
    desc->cspace = seL4_CapInitThreadCNode;
    desc->pd = seL4_CapInitThreadPD;
    desc->func = 0;
    desc->flags = 0;
    desc->arg0 = 0;
    desc->arg1 = 0;
    desc->arg2 = 0;
    desc->arg3 = 0;
    desc->use_remote_addr = false;
    desc->remote_src_cspace = seL4_CapInitThreadCNode;
    desc->remote_ep_addr = 0;
    desc->remote_ep_cap = 0;
    desc->remote_fault_ep_addr = 0;
}
