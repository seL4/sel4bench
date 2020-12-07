/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef __ARCH_HARDWARE_H
#define __ARCH_HARDWARE_H

#include <sel4_arch/hardware.h>

#define DO_NULLSYSCALL(swi) DO_NULLSYSCALL_OP(swi, seL4_SysBenchmarkNullSyscall)

#endif /* __ARCH_HARDWARE_H */
