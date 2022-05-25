#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(signal C)

set(configure_string "")
config_option(
    AppSignalBench
    APP_SIGNALBENCH
    "Application to benchmark seL4 signals without modification to the kernel."
    DEFAULT
    ON
    DEPENDS
    "DefaultBenchDeps"
)

add_config_library(sel4benchsignal "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(signal EXCLUDE_FROM_ALL ${deps})
target_link_libraries(signal sel4_autoconf sel4benchsignal_Config sel4benchsupport sel4muslcsys)

if(AppSignalBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:signal>")
endif()
