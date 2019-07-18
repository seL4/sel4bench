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

project(sel4benchapp C)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u __vsyscall_ptr")
set(configure_string "")

config_option(
    AllowUnstableOverhead
    ALLOW_UNSTABLE_OVERHEAD
    "Do not fail when stable values are not stable."
    DEFAULT
    OFF
)
config_option(
    OutputRawResults
    OUTPUT_RAW_RESULTS
    "As well as outputting statistics, dump raw results in JSON format."
    DEFAULT
    ON
)
config_string(
    JsonIndent
    JSON_INDENT
    "Set the indent for JSON. By default it is 0, which is very fast to output, however setting\
    the indent higher allows for more human readable output. Ranges from 0 to 31."
    DEFAULT
    0
    UNQUOTE
)
config_option(Sel4Bench SEL4_BENCH "Enable seL4 benchmarking" DEFAULT ON)

if(Sel4Bench)
    add_config_library(sel4benchapp "${configure_string}")

    file(GLOB static src/*.c src/plat/${KernelPlatform}/*.c)

    get_property(sel4benchapps GLOBAL PROPERTY sel4benchapps_property)
    MakeCPIO(archive.o "${sel4benchapps}")
    add_executable(sel4benchapp EXCLUDE_FROM_ALL ${static} archive.o)

    target_link_libraries(
        sel4benchapp
        jansson
        sel4bench
        sel4
        sel4muslcsys
        kernel_autoconf
        sel4benchapp_Config
        sel4benchfault_Config
        hardware_Config
        sel4benchipc_Config
        sel4benchirq_Config
        sel4benchirquser_Config
        sel4benchpagemapping_Config
        sel4benchscheduler_Config
        sel4benchsignal_Config
        smp_Config
        sel4benchsync_Config
        sel4benchvcpu_Config
    )

    DeclareRootserver(sel4benchapp)
endif()
