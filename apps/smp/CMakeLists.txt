#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

project(smp C)

set(KernelMaxNumNodesGreaterThan1 (KernelMaxNumNodes GREATER \"1\"))
set(configure_string "")
config_option(
    AppSmpBench
    APP_SMPBENCH
    "Enable SMP benchmarks"
    DEFAULT
    OFF
    DEPENDS
    "DefaultBenchDeps;KernelMaxNumNodesGreaterThan1"
)
add_config_library(smp "${configure_string}")

file(GLOB deps src/*.c)
add_executable(smp EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    smp
    sel4_autoconf
    smp_Config
    muslc
    sel4
    sel4benchsupport
    sel4platsupport
    sel4muslcsys
    jansson
    sel4sync
    sel4debug
    sel4allocman
    utils
    sel4utils
    sel4bench
    elf
    sel4simple
    cpio
    sel4vka
    sel4vspace
)

if(AppSmpBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:smp>")
endif()
