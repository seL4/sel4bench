/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SEL4BENCH_TIMING_H_
#define _SEL4BENCH_TIMING_H_

#include <assert.h>
#include <stdarg.h>
#include <sel4bench/sel4bench.h>

#ifndef TIMING_MULTIPLE_PMCS
    #define TIMING_MULTIPLE_PMCS true
#endif /* TIMING_MULTIPLE_PMCS */

#define TIMING_NUM_COUNTERS 4
#define TIMING_MAX_ID 128


/* Types */


/* Timing modes. */
typedef enum {
	timing_mode_label,
	timing_mode_enter,
	timing_mode_exit
} timing_point_mode_t;

/* Timing results print modes */
typedef enum {
    timing_print_raw,       /* Print raw sample data with no analysis. */
    timing_print_display,   /* Print human readable data. */
    timing_print_formatted, /* Print formatted tables designed to be easily 
                               script parsed for plotting. */
} timing_print_mode_t;

/* Stores a single timing point. */
struct timing_point {
    sel4bench_counter_t cycle_count;   /* CPU Cycle count */
	sel4bench_counter_t pmc0;          /* Counter 0 */
	sel4bench_counter_t pmc1;          /* Counter 1 */
	sel4bench_counter_t alignment;
    const char* name;
    unsigned int id;
    timing_point_mode_t mode;
};

typedef struct timing_point timing_point_t;

/* Stores everything about a timing sample list. */
struct timing_list {
    timing_point_t* points;
    unsigned int size;
    volatile timing_point_t* current_point;
    
    const char* title;
};

typedef struct timing_list timing_list_t;


/* Timing Library Functions */


/* Initalises timing system. Automatically called by first list creation if
 * uninitialised. */
void
timing_init(void);

/* Destroy timing system. Safe to print after destroyed. */
void
timing_destroy(void);

/* Destroy a list and deallocate all data points memory. */
void
timing_destroy_list(timing_list_t* list);

/* Create a list of a given maximum size. */
void
timing_create_list(timing_list_t* list, unsigned int size, const char* title);

/* Create a list of a given maximum size, using given pre-allocated points array.
 * The given array must be more 'size' points in size */
void
timing_create_list_points(timing_list_t* list, unsigned int size, const char* title,
        timing_point_t* points);

/* Add a sample to a list. Thread safe. Inlined for speed.
 * id is an integer 0 <= id <= TIMING_MAX_ID to uniquely identify the function name.
 * (Not thread-safe on ARMV5; no support for atomic fetch-and-inc.) */
static void inline __attribute__((always_inline))
timing_sample(timing_list_t* list, const char* name, unsigned int id, timing_point_mode_t mode)
{
    asm volatile(""); /* Compiler barrier start */
    
    timing_point_t* volatile temp = NULL;

    #ifdef ARMV5
        /* No support for atomic increment on ARMV5. */
        temp = list->current_point++;
    #else /* ARMV5 */
        /* Use atmoic increment to avoid race condition. */
        temp = (timing_point_t*)__sync_fetch_and_add(
                &list->current_point, sizeof(timing_point_t));
    #endif /* ARMV5 */

    temp->name = name;
    temp->id = id;
    temp->mode = mode;
    
    #if TIMING_MULTIPLE_PMCS
        /* Depends on the layout of timing_point_t */
        temp->cycle_count = sel4bench_get_counters(0x3, &temp->pmc0);
    #else /* TIMING_MULTIPLE_PMCS */
        temp->cycle_count = sel4bench_get_cycle_count();
    #endif /* TIMING_MULTIPLE_PMCS */

    asm volatile(""); /* Compiler barrier end */
}

/* Add a label to list. Thread Safe, inlined. */
#define timing_label(list, label, id) timing_sample(list, label, id, timing_mode_label)

/* Add a generic point to list. Thread Safe, inlined. */
#define timing_point(list, id) timing_sample(list, __FUNCTION__, id, timing_mode_label)

/* Add an entry point to list. Thread Safe, inlined. */
#define timing_entry(list, id) timing_sample(list, __FUNCTION__, id, timing_mode_enter)

/* Add an exit point to list. Thread Safe, inlined. */
#define timing_exit(list, id) timing_sample(list, __FUNCTION__, id, timing_mode_exit)

/* Prints the data points in a list. */
void
timing_print(const timing_list_t* list, const timing_print_mode_t print_mode);

/* Prints title for an experiment. */
void
timing_print_title(const timing_print_mode_t print_mode, const char *title_format, ...);

/* Retrieves a timing print, for if you want to do your own printing. Inlined for speed.
 * Use this instead of working with timing_list_t directly. */
static timing_point_t inline __attribute__((always_inline))
timing_getpoint(timing_list_t* list, unsigned int index)
{
    assert(list);
    assert(index < list->size);
    return list->points[index];
}

/* Retrieves the number of data points in a list. Inlined for speed.
 * Use this instead of working with timing_list_t directly. */
static unsigned int inline __attribute__((always_inline))
timing_getnumpoints(const timing_list_t* list) {
    assert(list);
    return list->current_point - list->points;
}

/* Retrieves the number of data points in a list. Inlined for speed.
 * Use this instead of working with timing_list_t directly. */
static unsigned int inline __attribute__((always_inline))
timing_getsize(const timing_list_t* list) {
    assert(list);
    return list->size;
}

#endif /* _SEL4TEST_TIMING_H_ */
