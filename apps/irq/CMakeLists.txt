#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(irq C)

set(configure_string "")
config_option(AppIrqBench APP_IRQBENCH "Application to benchmark seL4 IRQs from inside the kernel.\
    Requires tracepoints (see irq-tracepoint branch or kernel.\
    This app currently doesn't build." DEFAULT OFF DEPENDS "DefaultBenchDeps")
add_config_library(sel4benchirq "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(irq EXCLUDE_FROM_ALL ${deps})

if(AppIrqBench)
    target_link_libraries(irq sel4benchirq_Config sel4_autoconf sel4benchsupport)
endif()
