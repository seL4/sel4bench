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

project(sync C)

set(configure_string "")
config_option(
    AppSyncBench
    APP_SYNCBENCH
    "Application to benchmark seL4 sync."
    DEFAULT
    ON
    DEPENDS
    "DefaultBenchDeps"
)
add_config_library(sel4benchsync "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(sync EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    sync
    sel4_autoconf
    sel4benchsync_Config
    sel4benchsupport
    sel4sync
    sel4muslcsys
)

if(AppSyncBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:sync>")
endif()
