/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*
  Author: James Wilmot
  Description:
      Various IPC Benchmarking tests on seL4.
*/

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <twinkle/object_allocator.h>

#include "ipc.h"
#include "timing.h"
#include "environment.h"
#include "helpers.h"
#include "allocator_helpers.h"

#define VSPACE_PAGE_SIZE 4096

/* Arbitrary address for frame mapped
 * data array. */
#define DATA_ADDRESS 0x80000000
#define LIST_ADDRESS 0x70000000
#define MAX_DATA_POINTS 20000
#define DATA_SAMPLES 40

/* Debug printing. */
#if 0
    #define D(x...) printf(x)
#else
    #define D(x...)
#endif

typedef void (*test_func_t)(
    seL4_Word /* ep */,
    seL4_Word /* list */, 
    seL4_Word /* extra */
);

#define CSPACE_TIMING_POINTS (2 * BENCHMARK_MAXLENGTH * BENCHMARK_REPEATS)

/* Global timing list used for non-inter-vspace tests. */
timing_point_t global_timing_data[MAX_DATA_POINTS];
timing_list_t global_timing_list;

/* -------------- Declarations ---------- */

void experiment(void);

void benchmark_ipc_pair_overhead(void);
void print_benchmark_ipc_pair_overhead(timing_list_t* list);

void test_ipc_pair(test_func_t f1, test_func_t f2, bool inter_as, bool overhead);
void print_ipc_pair_results(timing_list_t* list, bool inter_as, 
        int min_prio, int max_prio, int prio_interval,
        int min_length, int max_length, int length_interval);

void call_func(seL4_Word endpoint, seL4_Word list, seL4_Word extra);
void replywait_func(seL4_Word endpoint, seL4_Word list, seL4_Word extra);

seL4_Word setup_timing_list(const char* list_name, struct allocator *current_allocator,
        bool inter_as, seL4_CPtr vspace1, seL4_CPtr vspace2);

void setup_address_spaces(struct allocator *alloc, 
        seL4_CPtr *cspace1, seL4_CPtr *cspace2, seL4_CPtr *vspace1, 
        seL4_CPtr *vspace2, bool inter_as, seL4_CPtr ep);
    
void do_test_ipc_pair(struct allocator *test_alloc, seL4_CPtr ep,  
        seL4_CPtr cspace1, seL4_CPtr cspace2,
        seL4_CPtr vspace1, seL4_CPtr vspace2,
        int prio1, int prio2, int length, 
        test_func_t f1, test_func_t f2,
        timing_list_t* list);

/* -------------- Definitions ---------- */

/* Run various IPC benchmarks. Enable/disable them here. */
void
vspace_benchmarks(void)
{
    experiment();
    benchmark_ipc_pair_overhead();
   
    printf("\n\n####### non-inter address space ######\n\n"); 
    reset_environment();
    test_ipc_pair(replywait_func, call_func, false, 0);
    
    printf("\n\n####### inter address space ######\n\n");
    reset_environment();
    test_ipc_pair(replywait_func, call_func, true, 0);
    
    reset_environment();
}

void
print_benchmark_ipc_pair_overhead(timing_list_t* list)
{
    assert(list!=NULL);
    int number_data_points = timing_getnumpoints(list);
    int delta;    

    printf("Number of data points collected: [%d]\n\n", number_data_points);
    for (int i = 0; i < number_data_points; i += 2) {
        delta = timing_getpoint(list, i+1).cycle_count -
                timing_getpoint(list, i  ).cycle_count; 
        printf("Iteration [%d] Timing overhead[%d]\n", i/2, delta);
    }
    printf("\n\n");
}

void
benchmark_ipc_pair_overhead(void)
{
    struct allocator *current_allocator;
    seL4_CPtr cspace1, cspace2;
    seL4_CPtr vspace1, vspace2;
    seL4_CPtr ep;
    timing_list_t list;
    
    current_allocator = get_allocator();

    ep = xalloc_kobject(current_allocator, seL4_EndpointObject, 0);

    setup_address_spaces(current_allocator, &cspace1, &cspace2, 
        &vspace1, &vspace2, false, ep);
    timing_create_list(&list, DATA_SAMPLES, "ipc_pair_overhead");
    
    for (int i = 0; i < DATA_SAMPLES / 2; i++) {
       timing_entry(&list, 0);
       timing_exit(&list, 0);
    }
    
    print_benchmark_ipc_pair_overhead(&list);
    timing_print(&list, timing_print_display);
    timing_destroy_list(&list);
}

void
setup_address_spaces(struct allocator *alloc, 
        seL4_CPtr *cspace1, seL4_CPtr *cspace2, seL4_CPtr *vspace1, 
        seL4_CPtr *vspace2, bool inter_as, seL4_CPtr ep)
{
    int error;
    
    if (inter_as) {
        *cspace1 = create_cspace(alloc);
        *cspace2 = create_cspace(alloc);
        *vspace1 = create_vspace(alloc, *cspace1);
        *vspace2 = create_vspace(alloc, *cspace2);
    } else {
        *cspace1 = seL4_CapInitThreadCNode;
        *cspace2 = seL4_CapInitThreadCNode;
        *vspace1 = seL4_CapInitThreadPD;
        *vspace2 = seL4_CapInitThreadPD;
    }

    if (inter_as) {
        error = seL4_CNode_Copy(
                *cspace1, ep, seL4_WordBits,
                seL4_CapInitThreadCNode, ep, seL4_WordBits,
                seL4_AllRights);
        assert(!error);

        error = seL4_CNode_Copy(
                *cspace2, ep, seL4_WordBits,
                seL4_CapInitThreadCNode, ep, seL4_WordBits,
                seL4_AllRights);
        assert(!error);
    }
}

void
test_ipc_pair(test_func_t f1, test_func_t f2, bool inter_as, bool overhead)
{
    struct allocator *current_allocator;
    seL4_CPtr cspace1, cspace2;
    seL4_CPtr vspace1, vspace2;
    struct allocator test_alloc;
    int prio1, prio2;
    int error; 
    seL4_CPtr ep;
    timing_list_t* list;

    int min_prio = 80;
    int max_prio = 84;
    int prio_interval = 2;

    int min_length = 0;
    int max_length = 16;
    int length_interval = 2;

    int length = 0;

    current_allocator = get_allocator();    
    ep = xalloc_kobject(current_allocator, seL4_EndpointObject, 0);

    setup_address_spaces(current_allocator, &cspace1, &cspace2, 
            &vspace1, &vspace2, inter_as, ep);

    /* Set up timing list. */
    list = (timing_list_t*) setup_timing_list("test_ipc_pair", current_allocator,
            inter_as, vspace1, vspace2);

    /* Use a child allocator for each test run.  */
    setup_child_allocator(current_allocator, &test_alloc); 

    for (prio1 = min_prio; prio1 <= max_prio; prio1 += prio_interval) {
        for (prio2 = min_prio; prio2 <= max_prio; prio2 += prio_interval) {
            for (length = min_length; length <= max_length; length += length_interval) { 
                D("test_ipc_pair: length [%d]\n", length);
                do_test_ipc_pair(&test_alloc, ep, cspace1, cspace2, 
                    vspace1, vspace2, prio1, prio2, length, f1, f2, list);
                allocator_reset(&test_alloc);
            }
        }
    }
    
    /* Display results. */
    print_ipc_pair_results(list, inter_as, min_prio, max_prio, prio_interval,
            min_length, max_length, length_interval); 
    /*timing_print(&list, timing_print_display);*/

    /* Clean up. */
    error = seL4_CNode_Delete(seL4_CapInitThreadCNode,
            ep, seL4_WordBits);
    assert(!error);
}

void
print_ipc_pair_results(timing_list_t* list, bool inter_as, 
        int min_prio, int max_prio, int prio_interval,
        int min_length, int max_length, int length_interval)
{
    assert(list!=NULL);
    int prio1, prio2, length;
    int number_data_points = timing_getnumpoints(list);
    int iteration = 0, offset = 0, i = 0, delta = 0;

    printf("*****************************************\n");
    printf("Inter addr space: [%s]\n", inter_as ? "Yes" : "No");
    printf("Number of data points collected: [%d]\n\n", number_data_points);
    
    for (prio1 = min_prio; prio1 <= max_prio; prio1 += prio_interval) {
        for (prio2 = min_prio; prio2 <= max_prio; prio2 += prio_interval) {
            for (length = min_length; length <= max_length; length += length_interval) { 
                printf("Call prio [%d] -> ReplyWait prio [%d]\n", prio2, prio1);
                printf("Length [%d]\n", length);
                printf("Length interval %d\n", length_interval);
                printf("Raw data:\n");
                
                /* Shortcut to find out where we are in data. */
                offset = iteration * DATA_SAMPLES + 1;

                /* += 4 because the samples come in pairs. */
                for (i = 0; i < DATA_SAMPLES; i+= 4) {
                    delta = timing_getpoint(list, i + offset + 1).cycle_count -
                            timing_getpoint(list, i + offset    ).cycle_count;
                    printf("One way call/wait: [%d] count delta [%d]\n", i/4, delta);

                    delta = timing_getpoint(list, i + offset + 3).cycle_count -
                            timing_getpoint(list, i + offset + 2).cycle_count;
                    printf("One way reply: [%d] count delta [%d]\n\n", i/4, delta);
                }
                printf("\n");
                iteration++; 
            }
        }
    } 
} 

void
allocate_map_pages_inter_as(struct allocator *current_allocator, seL4_Word starting_address,
        int size_pages, seL4_CPtr vspace1, seL4_CPtr vspace2)
{
    int error;
    for (int i = 0; i < size_pages; i++) {
        seL4_CPtr frame = xalloc_kobject(current_allocator, seL4_ARCH_4KPage, 0);
        seL4_CPtr frame_copy1 = allocator_alloc_cslot(current_allocator);  
        seL4_CPtr frame_copy2 = allocator_alloc_cslot(current_allocator);  

        /* this hackyness is needed because you can't map the same cslot 
         * for a frame into multiple page directories (see the shared memory
         * part of the manual for more information)*/
        error = seL4_CNode_Copy(seL4_CapInitThreadCNode, frame_copy1, 
                seL4_WordBits, seL4_CapInitThreadCNode,
                frame, seL4_WordBits, seL4_AllRights);
        assert(!error);
        
        error = seL4_CNode_Copy(seL4_CapInitThreadCNode, frame_copy2, 
                seL4_WordBits, seL4_CapInitThreadCNode,
                frame, seL4_WordBits, seL4_AllRights);
        assert(!error);
        
        /* Map into first address space. */
        map_frame_into_pd(current_allocator, 
            vspace1, frame, starting_address + (i * 4096), 
            seL4_AllRights, true); 
       
        /* Map into second address space. */
        map_frame_into_pd(current_allocator, 
            vspace2, frame_copy1, starting_address + (i * 4096), 
            seL4_AllRights, true); 

        /* Map into init threads address space. */
        map_frame_into_pd(current_allocator, 
            seL4_CapInitThreadPD, frame_copy2, starting_address + (i * 4096), 
            seL4_AllRights, true); 
    } 
}

static unsigned int inline __attribute__((always_inline))
calc_page_size(size_t size) {
    /* There must exist a macro to convert from bytes to pagesize? */
    return (size / VSPACE_PAGE_SIZE) + 1;
}

seL4_Word
setup_timing_list(const char* list_name, struct allocator *current_allocator, bool inter_as, 
        seL4_CPtr vspace1, seL4_CPtr vspace2)
{ 
    seL4_Word tlist, tlist_data;
    if (inter_as) {
        tlist = LIST_ADDRESS; 
        tlist_data = DATA_ADDRESS;   
        /* Map data block in thread vspaces. */
        unsigned int data_pagesize = calc_page_size(sizeof(timing_point_t) * MAX_DATA_POINTS);
        allocate_map_pages_inter_as(current_allocator, DATA_ADDRESS,
                data_pagesize, vspace1,vspace2); 
        /* Map list object into thread vspaces. */
        unsigned int list_pagesize = calc_page_size(sizeof(timing_list_t));
        allocate_map_pages_inter_as(current_allocator, LIST_ADDRESS,
                list_pagesize, vspace1, vspace2);
        
    } else {
        /* Just use the global data array and global list object. */
        tlist = (seL4_Word) (&global_timing_list); 
        tlist_data = (seL4_Word) (&global_timing_data);      
    }
    /* Initialise list object. */
    timing_create_list_points((timing_list_t*)tlist, MAX_DATA_POINTS, list_name,
            (timing_point_t*)tlist_data);
    return tlist;     
}

/* Sets up and runs benchmarking test on a pair of threads IPC-ing each other. */
void
do_test_ipc_pair(struct allocator *test_alloc, seL4_CPtr ep,  
        seL4_CPtr cspace1, seL4_CPtr cspace2,
        seL4_CPtr vspace1, seL4_CPtr vspace2,
        int prio1, int prio2, int length, 
        test_func_t f1, test_func_t f2,
        timing_list_t* list)
{
    helper_thread_t thread1, thread2;
    
    create_helper_thread_in(&thread1, test_alloc, cspace1, vspace1,
            (helper_func_t)f1, 0,
            ep, (seL4_Word)list, length, 0);
    set_helper_thread_name(&thread1, "IPC Thread A");

    create_helper_thread_in(&thread2, test_alloc, cspace2, vspace2,
            (helper_func_t)f2, 0,
            ep, (seL4_Word)list, length, 0);
    set_helper_thread_name(&thread2, "IPC Thread B");

    set_helper_thread_priority(&thread1, prio1);
    set_helper_thread_priority(&thread2, prio2);
    
    /* Threads are enqueued at the head of the scheduling queue, so the
     * thread enqueued last will be run first, for a given priority. */
    start_helper_thread(&thread1);
    start_helper_thread(&thread2);

    wait_for_thread(&thread1);
    wait_for_thread(&thread2);
    
    cleanup_helper_thread(&thread1);
    cleanup_helper_thread(&thread2);
}

/* Call method used be helper thread for benchmarking. */
void
call_func(seL4_Word endpoint, seL4_Word list, seL4_Word extra)
{
    int length = (int)extra;
    timing_list_t* plist = (timing_list_t*) list;

    D("call_func: length [%d]\n", length);

    seL4_MessageInfo tag = { {.length = length } };

    /* Construct a message. */
    for (int i = 0; i < length; i++) {
         D("*** %d\n", i);
         seL4_SetMR(i, 1337+i*3);
    }
    
    tag = seL4_Call (endpoint, tag);

    for (int i = 0; i < DATA_SAMPLES; i++) {
        D("before call\n");
        /* 0 */
        timing_point(plist, 0);
        tag = seL4_Call (endpoint, tag);
        /* 3 */
        timing_point(plist, 0);
        D("after call\n");
    }
}

/* Replywait method used be helper thread for benchmarking. */
void
replywait_func(seL4_Word endpoint, seL4_Word list, seL4_Word extra)
{
    seL4_MessageInfo tag;
    seL4_Word sender_badge = 0;
    timing_list_t* plist = (timing_list_t*) list;
    
    /* First reply/wait can't reply. */
    tag = seL4_Wait(endpoint, &sender_badge);
    
    for (int i = 0; i < DATA_SAMPLES; i++) {
        D("before replywait\n");
        /* 2 */
        timing_point(plist, 1);
        tag = seL4_ReplyWait(endpoint, tag, &sender_badge);
        /* 1 */
        timing_point(plist, 1);
        D("after replywait\n");
    }

    seL4_Reply(tag);
}

/* Runs a simple benchmark experiment. */
void
experiment(void)
{
    struct allocator *current_allocator;
    seL4_CPtr cspace1, cspace2;
    seL4_CPtr vspace1, vspace2;
    seL4_CPtr ep;
    
    timing_list_t* list = NULL;

    /* Set up experiment. */
    current_allocator = get_allocator();    
    ep = xalloc_kobject(current_allocator, seL4_EndpointObject, 0);

    setup_address_spaces(current_allocator, &cspace1, &cspace2, 
        &vspace1, &vspace2, false, ep);
        
    list = (timing_list_t*) setup_timing_list("experiment", current_allocator, false,
            vspace1, vspace2);
    
    int i = 0, j = 0;
    /* Run the benchmark experiment. */
    timing_entry(list, 0);
    for (i = 0; i < 100000; i++) {
        j++;
        asm("");
    }
    timing_exit(list, 0);

    /* Output results */
    printf("Delta [%d]\n", timing_getpoint(list, 1).cycle_count - 
                           timing_getpoint(list, 0).cycle_count);
    timing_print(list, timing_print_display);
}

/* -------------- Unused / Deprecated code ---------- */

#if 0
/* Setup the data frame for inter address space benchmarking. */
void
setup_data_frame_interas(struct allocator *current_allocator, seL4_CPtr pd1, seL4_CPtr pd2)
{    
    seL4_CPtr data_frame;
    /* create frame for writing results into  */
    data_frame = xalloc_kobject(current_allocator, seL4_ARCH_4KPage, 0);

    /* map frame into thread 1's address space */
    map_frame_into_pd(current_allocator, pd1, data_frame, 
        (seL4_Word)DATA_ADDRESS, seL4_AllRights, true);

    /* map frame into thread 2's address space */
    map_frame_into_pd(current_allocator, pd2, data_frame, 
        (seL4_Word)DATA_ADDRESS, seL4_AllRights, true);

    /* map frame into init thread's address space
     * which happens to be the sel4bench app
     * means we can print results from app thread */
    map_frame_into_pd(current_allocator, seL4_CapInitThreadPD, data_frame, 
        (seL4_Word)DATA_ADDRESS, seL4_AllRights, true);
}

void
benchmark_call_replywait_inter_as()
{
    struct allocator *current_allocator;
    seL4_CPtr call_thread_pd, replywait_thread_pd;
    helper_thread_t call_thread, replywait_thread;   

    /* Setup an endpoint for messaging */
    seL4_CPtr ep = xalloc_kobject(current_allocator, seL4_EndpointObject, 0);
 
    current_allocator = get_allocator();    

    setup_address_spaces(current_allocator, &call_thread_pd, &replywait_thread_pd, seL4_CPtr ep);
    setup_data_frame_interas(current_allocator, call_thread_pd, replywait_thread_pd);    
 
}

void
setup_data_inter_as(struct allocator *current_allocator, timing_data_point_t **data,
        seL4_CPtr vspace1, seL4_CPtr vspace2)
{
    int error; 
    int i;

    for (i=0; i<size; i++) {
        seL4_CPtr frame = xalloc_kobject(current_allocator, seL4_ARCH_4KPage, 0);
        seL4_CPtr frame_copy1 = allocator_alloc_cslot(current_allocator);  
        seL4_CPtr frame_copy2 = allocator_alloc_cslot(current_allocator);  

        /* this hackyness is needed because you can't map the same cslot 
         * for a frame into multiple page directories (see the shared memory
         * part of the manual for more information)*/
        error = seL4_CNode_Copy(seL4_CapInitThreadCNode, frame_copy1, 
            seL4_WordBits, seL4_CapInitThreadCNode,
            frame, seL4_WordBits, seL4_AllRights);
        
        assert(!error);
        
        error = seL4_CNode_Copy(seL4_CapInitThreadCNode, frame_copy2, 
            seL4_WordBits, seL4_CapInitThreadCNode,
            frame, seL4_WordBits, seL4_AllRights);
        
        assert(!error);
        

        /* map into first address space */
        map_frame_into_pd(current_allocator, 
            vspace1, frame, DATA_ADDRESS + (i * 4096), 
            seL4_AllRights, true); 
       
        /* map into second address space */
        map_frame_into_pd(current_allocator, 
            vspace2, frame_copy1, DATA_ADDRESS + (i * 4096), 
            seL4_AllRights, true); 

        /* map into init threads address space */
        map_frame_into_pd(current_allocator, 
            seL4_CapInitThreadPD, frame_copy2, DATA_ADDRESS + (i * 4096), 
            seL4_AllRights, true); 
    } 

}
#endif

#endif

