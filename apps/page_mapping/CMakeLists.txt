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

project(page_mapping C)

set(configure_string "")
config_option(
    AppPageMappingBench
    APP_PAGEMAPPINGBENCH
    "Application to benchmark seL4 mapping a series of pages."
    DEFAULT
    ON
    DEPENDS
    "DefaultBenchDeps"
)
add_config_library(sel4benchpagemapping "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(page_mapping EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    page_mapping
    sel4_autoconf
    sel4benchpagemapping_Config
    sel4
    muslc
    cpio
    sel4vka
    sel4vspace
    sel4allocman
    sel4utils
    sel4muslcsys
    elf
    sel4simple
    sel4bench
    sel4benchsupport
    sel4debug
    platsupport
    sel4platsupport
)

if(AppPageMappingBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:page_mapping>")
endif()
