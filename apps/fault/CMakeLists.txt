#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(fault C)

set(configure_string "")
config_option(
    AppFaultBench
    APP_FAULTBENCH
    "Application to benchmark seL4 faults without modification to the kernel."
    DEFAULT
    OFF
    DEPENDS
    "DefaultBenchDeps"
)
add_config_library(sel4benchfault "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(fault EXCLUDE_FROM_ALL ${deps})
if("${PLATFORM}" STREQUAL "sabre")
    target_compile_options(fault PRIVATE -O1)
endif()
target_link_libraries(
    fault
    sel4_autoconf
    sel4benchfault_Config
    sel4
    sel4bench
    muslc
    sel4vka
    utils
    elf
    sel4allocman
    sel4utils
    sel4simple
    sel4muslcsys
    sel4platsupport
    platsupport
    sel4vspace
    sel4benchsupport
    sel4debug
)

if(AppFaultBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:fault>")
endif()
