  /*
   * Copyright 2016, NICTA
   *
   * This software may be distributed and modified according to the terms of
   * the BSD 2-Clause license. Note that NO WARRANTY is provided.
   * See "LICENSE_BSDv2.txt" for details.
   *
   * @TAG(NICTA_BSD)
   */
#ifndef __BENCHMARK_SUPPORT_H
#define __BENCHMARK_SUPPORT_H

#include <sel4utils/slab.h>
#include <simple/arch/simple.h>

void benchmark_arch_get_simple(arch_simple_t *simple);
void benchmark_arch_get_timers(env_t *env);

#endif /* __BENCHMARK_SUPPORT_H */
