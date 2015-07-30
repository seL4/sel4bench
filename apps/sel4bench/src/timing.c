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
  Author: Xi (Ma) Chen
  Created: Tue April 24 2012
  Description:
      Timing library based upon libseL4bench, which takes
      samples of CPU cycles and optionally other PMC counters.
      
      Based on old timing.h module by justinkl.
      
      Features include keeping separate multiple lists, thread-safe sample
      recording (not supported on ARMv5), sensible general-purpose list displaying
      (with ANSI colours), and interface for accessing individual data samples
      from list if for custom printing / analysis.
      
      Timers are quite expensive, ~300 cycles.
*/

#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "util/ansi.h"

static char timing_ready = false;

void
timing_init(void)
{
    sel4bench_init();

    #if TIMING_MULTIPLE_PMCS
        /* Enable PMC0 and PMC1 if asked for */
        assert(sel4bench_get_num_counters() >= 2);
        sel4bench_set_count_event(0, SEL4BENCH_EVENT_CACHE_L1D_MISS);
        sel4bench_set_count_event(1, SEL4BENCH_EVENT_CACHE_L1I_MISS);
        sel4bench_start_counters(0x3);
        sel4bench_reset_counters(0x3);
    #endif /* TIMING_MULTIPLE_PMCS */

    timing_ready = true;
}

void
timing_destroy(void)
{
    sel4bench_destroy();
    timing_ready = false;
}

void
timing_destroy_list(timing_list_t* list)
{
    assert(list);
    assert(list->points);
    free(list->points);
}

void
timing_create_list(timing_list_t* list, unsigned int size, const char* title)
{
    assert(list);
    
    if (!timing_ready) timing_init();

    list->points = (timing_point_t*) malloc(size * sizeof(timing_point_t));
    list->size = size;
    list->current_point = list->points;
    memset(list->points, 0, size * sizeof(timing_point_t));
    
    list->title = title;
}

void
timing_create_list_points(timing_list_t* list, unsigned int size, const char* title,
        timing_point_t* points)
{
    assert(list);
    assert(points);
    
    if (!timing_ready) timing_init();
    
    list->points = points;
    list->size = size;
    list->current_point = list->points;
    memset(list->points, 0, size * sizeof(timing_point_t));
    
    list->title = title;
}

void
timing_print(const timing_list_t* list, const timing_print_mode_t print_mode)
{
    assert(list);
    
    unsigned int num_points = timing_getsize(list);
    unsigned int sample = 0;
    timing_point_t* dp = NULL;
    timing_point_t* entry_dp[TIMING_MAX_ID + 1] = {};
    
    switch (print_mode) {
        case timing_print_raw:
            /* Print raw sample data with no analysis. */
            printf("======  %s ======\n", list->title);
            printf("Total timing points: %d\n", num_points);
            
            printf("#Sample, Cycles, PMC0, PMC1, Mode, Name\n");
            printf("---------------------------------------------\n");
            
            for(sample = 0, dp = list->points;
                    dp != list->current_point;
                    dp++, sample++) {
                const char* mode_str =  (dp->mode == timing_mode_enter) ? "enter" :
		                               ((dp->mode == timing_mode_exit ) ? "exit"  :
		                                                                  "none" );
                const char* name_str = dp->name ? dp->name : "none";
                
                printf("%u/%u - ", sample, num_points);
                printf("%llu, %llu, %llu, %s, %s\n",
                        (uint64_t) dp->cycle_count,
                        (uint64_t) dp->pmc0,
                        (uint64_t) dp->pmc1,
                        mode_str,
                        name_str);
            }
            break;
        case timing_print_display:
            /* Print coloured human readable data. */
            printf("%s%s============== %s ==============%s\n",
                    A_RESET, A_FG_W, list->title, A_RESET);
            
            for(dp = list->points; dp != list->current_point; dp++) {
                switch (dp->mode) {
                    case timing_mode_enter:
                        assert(dp->id <= TIMING_MAX_ID);
                        entry_dp[dp->id] = dp;
                        break;
                    case timing_mode_exit:
                        assert(dp->id <= TIMING_MAX_ID);
                        if (entry_dp[dp->id]) {
                            printf("%d - %s%sID %d %s- %s%llu Cycles, %s%llu PMC0, "
                                   "%s%llu PMC1 %s\n",
                                    sample++, A_FG_W, A_BOLD,
                                    dp->id, A_RESET, A_FG_Y,
                                    (uint64_t)(dp->cycle_count - entry_dp[dp->id]->cycle_count),
                                    A_FG_R,
                                    (uint64_t)(dp->pmc0 - entry_dp[dp->id]->pmc0),
                                    A_FG_C,
                                    (uint64_t)(dp->pmc1 - entry_dp[dp->id]->pmc1),
                                    A_RESET);
                        } else {
                            /* Corresponding entry point not found. */
                        }
                        break;
                    default: break;
                }
            }
            break;
        case timing_print_formatted:
            printf("!! %s\n", list->title);
            for(dp = list->points; dp != list->current_point; dp++) {
                switch (dp->mode) {
                    case timing_mode_enter:
                        entry_dp[dp->id] = dp;
                        break;
                    case timing_mode_exit:
                        if (entry_dp[dp->id]) {
                            printf("! %llu, %llu, %llu\n",
                                 (uint64_t)(dp->cycle_count - entry_dp[dp->id]->cycle_count),
                                 (uint64_t)(dp->pmc0 - entry_dp[dp->id]->pmc0),
                                 (uint64_t)(dp->pmc1 - entry_dp[dp->id]->pmc1));
                        }
                        break;
                    default: break;
                }
            }
            break;
        default:
            printf("timing_print: invalid print mode.\n");
            assert(0);
            break;
    }
}

void
timing_print_title(const timing_print_mode_t print_mode, const char *title_format, ...)
{
    va_list ap;
    switch (print_mode) {
        case timing_print_raw:
            printf("\n=====< ");
              va_start(ap, title_format);
                vprintf(title_format, ap);
              va_end(ap);
            printf(" >=====\n");
            break;
        case timing_print_display:
            printf("\n%s%s%s=====< ", A_RESET, A_FG_W, A_BOLD);
              va_start(ap, title_format);
                vprintf(title_format, ap);
              va_end(ap);
            printf(" >=====%s\n", A_RESET);
            break;
        case timing_print_formatted:
            printf("\n!!! ");
            va_start(ap, title_format);
              vprintf(title_format, ap);
            va_end(ap);
            printf("\n");
            break;
        default:
            printf("timing_print_title: invalid print mode.\n");
            assert(0);
            break;
    }
}

