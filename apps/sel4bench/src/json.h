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

#include "benchmark.h"
#include <jansson.h>
#include <sel4bench/sel4bench.h>

json_t *result_set_to_json(result_set_t set);

