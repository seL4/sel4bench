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
