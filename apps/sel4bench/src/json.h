/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include "benchmark.h"
#include <jansson.h>
#include <sel4bench/sel4bench.h>
#include <benchmark.h>

json_t *result_set_to_json(result_set_t set);
json_t *average_counters_to_json(char *name, result_t counters[NUM_AVERAGE_EVENTS]);
