/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "benchmark.h"
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <benchmark.h>

json_t *result_set_to_json(result_set_t set);
json_t *average_counters_to_json(char *name, result_t counters[NUM_AVERAGE_EVENTS]);
