/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "benchmark.h"

result_t calculate_results(const size_t n, ccnt_t data[n]);

result_t calculate_results_early_proc(ccnt_t num, ccnt_t min, ccnt_t max,
                                      ccnt_t sum, ccnt_t sum2, ccnt_t array[num]);

static double results_variance_early_proc(const size_t n, const ccnt_t sum,
                                          const ccnt_t sum2, const ccnt_t mean);
