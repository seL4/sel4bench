#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(irquser C)

set(configure_string "")
config_option(
    AppIrqUserBench
    APP_IRQUSERBENCH
    "Application to benchmark seL4 IRQs without modification to the kernel."
    DEFAULT
    ON
    DEPENDS
    "DefaultBenchDeps"
)
add_config_library(sel4benchirquser "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(irquser EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    irquser
    sel4_autoconf
    sel4benchirquser_Config
    sel4
    muslc
    sel4vka
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

if(AppIrqUserBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:irquser>")
endif()
