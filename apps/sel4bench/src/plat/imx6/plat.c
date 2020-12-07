/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <platsupport/clock.h>
#include <utils/frequency.h>

#include "../../env.h"

#define IMX6_MAX_FREQ (996 * MHZ)

void plat_setup(env_t *env)
{
    /* set to highest cpu freq */
    int error = sel4platsupport_new_io_mapper(&env->vspace, &env->vka, &env->ops.io_mapper);
    ZF_LOGF_IF(error != 0, "Failed to get io mapper");

    clock_sys_t clock;
    error = clock_sys_init(&env->ops, &clock);
    ZF_LOGF_IF(error != 0, "clock_sys_init failed");

    clk_t *clk = clk_get_clock(&clock, CLK_ARM);
    ZF_LOGF_IF(error != 0, "Failed to get clock");

    freq_t freq = clk_set_freq(clk, IMX6_MAX_FREQ);
    ZF_LOGF_IF(freq != IMX6_MAX_FREQ, "Failed to set imx6 freq");
}
