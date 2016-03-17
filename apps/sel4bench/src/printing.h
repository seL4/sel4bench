/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef __SEL4BENCH_PRINTING_H
#define __SEL4BENCH_PRINTING_H

void print_all(ccnt_t *array, int size);

void print_result_header(void);
void print_result(result_t *result);

#endif /* __SEL4BENCH_PRINTING_H */
