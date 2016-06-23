/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#pragma once

#include <sel4bench/sel4bench.h>

typedef enum {
   XML,
   TSV
} format_t;

void print_banner(char *name, int samples);
void print_all(ccnt_t *array, int size);
void print_result_header(void);
void print_result(result_t *result);

