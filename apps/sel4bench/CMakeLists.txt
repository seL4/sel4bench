#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(sel4benchapp C)
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

# Default dependencies on kernel benchmarking features. Declared here so that
# all the benchmark applications can use it
if(
    (KernelArchX86 AND KernelExportPMCUser AND KernelX86DangerousMSR)
    OR (KernelArchARM AND KernelArmExportPMUUser)
    OR (KernelArchArmCortexA8 AND KernelDangerousCodeInjection)
)
    set(DefaultBenchDeps TRUE)
else()
    set(DefaultBenchDeps FALSE)
endif()

find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)
find_package(sel4_projects_libs REQUIRED)
find_package(projects_libs REQUIRED)

# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
set(LibSel4MuslcSysMorecoreBytes 0 CACHE STRING "" FORCE)
sel4_libs_import_libraries()
projects_libs_import_libraries()
set(LibNanopb ON CACHE BOOL "" FORCE)
sel4_projects_libs_import_libraries()

add_subdirectory(../fault fault)
add_subdirectory(../hardware hardware)
add_subdirectory(../ipc ipc)
add_subdirectory(../irq irq)
add_subdirectory(../irquser irquser)
add_subdirectory(../page_mapping page_mapping)
add_subdirectory(../scheduler scheduler)
add_subdirectory(../signal signal)
add_subdirectory(../smp smp)
add_subdirectory(../sync sync)
add_subdirectory(../vcpu vcpu)
# Add new benchmark applications here

add_subdirectory(../../libsel4benchsupport libsel4benchsupport)

config_option(Sel4Bench SEL4_BENCH "Enable seL4 benchmarking" DEFAULT ON)

if(Sel4Bench)
    add_config_library(sel4benchapp "${configure_string}")

    file(GLOB static src/*.c src/plat/${KernelPlatform}/*.c)

    get_property(sel4benchapps GLOBAL PROPERTY sel4benchapps_property)
    include(cpio)
    MakeCPIO(archive.o "${sel4benchapps}")
    add_executable(sel4benchapp EXCLUDE_FROM_ALL ${static} archive.o)

    target_link_libraries(
        sel4benchapp
        jansson
        sel4benchsupport
        sel4
        sel4muslcsys
        sel4rpc
        sel4_autoconf
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
        # Add new benchmark configs here
    )
    include(rootserver)
    DeclareRootserver(sel4benchapp)
endif()
