/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TESTS_H_
#define _TESTS_H_

#include <vka/vka.h>
#include <simple/simple.h>

/* Maximum number of test cases we support. */
#define MAX_TESTS 128

/* Contains information about the test environment. */
struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    simple_t *simple;
    /* an initialised allocator that we can create child allocators from */
    struct allocator* allocator;
};

#endif /* _TESTS_H_ */
