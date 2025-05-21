#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(vcpu C)

set(configure_string "")
config_option(
    AppVcpuBench
    APP_VCPU_BENCH
    "Application to benchmark seL4 VCPU performance."
    DEFAULT
    OFF
    DEPENDS
    "DefaultBenchDeps;KernelSel4ArchAarch64;KernelArmHypervisorSupport;KernelBenchmarksTrackKernelEntries"
)
add_config_library(sel4benchvcpu "${configure_string}")

file(GLOB deps src/*.c src/guest_asm.S)
list(SORT deps)
add_executable(vcpu EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    vcpu
    sel4_autoconf
    sel4benchvcpu_Config
    sel4
    muslc
    cpio
    sel4vka
    sel4vspace
    sel4allocman
    sel4utils
    elf
    sel4simple
    sel4benchsupport
    sel4debug
    platsupport
    sel4platsupport
    sel4muslcsys
)

if(AppVcpuBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:vcpu>")
endif()
