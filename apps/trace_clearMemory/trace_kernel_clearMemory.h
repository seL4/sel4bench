/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __ASSEMBLER__
#include <types.h>

/* This header file gets inserted at the start of each of the kernel's source files.
 * It will cause every call to clearMemory to be replaced with a call to clearMemoryOverride
 * which takes our tracing measurements and then calls the real clearMemory function.
 * because the kernel is compiled as a single source file, and all these functions are defined
 * as static inline, the actual generated assembly after inlining should be the same as if we
 * inserted the tracepoint assembly directly into clearMemory and therefore have minimal overhead.
 * This does require us to slightly modify the real declaration of clearMemory to prevent it from
 * also being preprocessed. (changing void clearMemory(args) to void (clearMemory)(args).)
 */

/* Forward declare the trace point functions */
static inline void trace_point_start(word_t id);
static inline void trace_point_stop(word_t id);

/* Forward declare the clearMemory function that we are measuring */
static inline void clearMemory(word_t *ptr, word_t bits);

/* Implement our override function.  This calls the real function
   within a trace start and stop. */
static inline void clearMemoryOverride(word_t *ptr, word_t bits) {
    trace_point_start(0);
    trace_point_stop(0);
    trace_point_start(1);
	clearMemory(ptr, bits);
    trace_point_stop(1);
}

/* Override all calls to clearMemory with clearMemoryOverride */
#define clearMemory(a, b) clearMemoryOverride(a, b)

#endif

