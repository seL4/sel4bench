/*
 * Copyright 2026, STMicroelectronics
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../../env.h"
#include <iwdg.h>

void plat_setup(env_t *env)
{
#ifdef CONFIG_ALLOW_SMC_CALLS
    seL4_Error error;

    error = stm32_iwdg_enable(false);
    ZF_LOGE_IF(error != seL4_NoError, "Failed to disable IWDG (error %d)", error);
#else
    ZF_LOGW("CONFIG_ALLOW_SMC_CALLS not enabled");
#endif
}
