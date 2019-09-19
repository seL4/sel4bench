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

project(hardware C)

set(configure_string "")
config_option(
    AppHardwareBench
    APP_HARDWAREBENCH
    "Application to benchmark hardware-related operations."
    DEFAULT
    OFF
    DEPENDS
    "KernelEnableBenchmarks;DefaultBenchDeps"
)
add_config_library(hardware "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(hardware EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    hardware
    hardware_Config
    sel4
    muslc
    platsupport
    sel4platsupport
    sel4benchsupport
    elf
    sel4utils
    sel4simple
    sel4bench
    utils
    sel4debug
    sel4muslcsys
)
if(AppHardwareBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:hardware>")
endif()
