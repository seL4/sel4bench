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

result_t process_result(ccnt_t *array, int size, const char *error);
result_t process_result_ignored(ccnt_t *array, int size, int ignored, const char *error);

