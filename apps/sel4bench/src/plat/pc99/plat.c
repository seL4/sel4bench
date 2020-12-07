/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../../env.h"

void plat_setup(env_t *env)
{
    if (config_set(CONFIG_ARCH_X86_64)) {
        /* check if the cpu supports rdtscp */
        int edx = 0;
        asm volatile("cpuid":"=d"(edx):"a"(0x80000001):"ecx");
        ZF_LOGF_IF((edx & (BIT(27))) == 0, "CPU does not support rdtscp instruction");
    }
}
