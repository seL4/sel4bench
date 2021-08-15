#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(trace_clearmemory C)

set(configure_string "")
config_option(AppTraceClearMemoryBench APP_TRACE_CLEARMEMORY_BENCH "Application to measure clearMemory cycle count from inside the kernel.\
    Requires tracepoints." DEFAULT OFF)
add_config_library(sel4benchtraceclearmemory "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(traceclearmemory EXCLUDE_FROM_ALL ${deps})

target_link_libraries(traceclearmemory sel4benchtraceclearmemory_Config sel4_autoconf sel4benchsupport)

if(AppTraceClearMemoryBench)

    if (NOT "${KernelBenchmarks}" STREQUAL "tracepoints")
        message(FATAL_ERROR "This app requires KernelBenchmarks to be set to tracepoints")
    endif()
    if (NOT "${KernelMaxNumTracePoints}" STREQUAL "2")
        message(FATAL_ERROR "This app requires KernelMaxNumTracePoints to be set to 2")
    endif()
    if (NOT "${KernelArch}" STREQUAL "arm")
        message(FATAL_ERROR "This app only runs on arm platforms.")
    endif()
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:traceclearmemory>")

    # Modify some of the build rules for kernel.elf to insert our tracing code into the kernel sources before it is built.
    # This strategy allows us to attach trace points around the function we want to trace at the source level using the C preprocessor in a way
    # that doesn't require changing the kernel sources in the kernel repository while also introducing minimal runtime overhead.
    # The tracepoints are dynamically enabled or disabled at runtime if the kernel tracing buffer is running.
    # It isn't expected that these tracepoints are going to be used in a production environment and so are compiled in.

    # Rewrite the definition of clearMemory to add brackets around the function name. This prevents it from being rewritten by the preprocessor
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel/arch/machine.h
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/kernel/arch/
        COMMAND sed -e "'s/void clearMemory(/void (clearMemory)(/'" ${KERNEL_PATH}/include/arch/arm/arch/machine.h > ${CMAKE_CURRENT_BINARY_DIR}/kernel/arch/machine.h
        DEPENDS ${KERNEL_PATH}/include/arch/arm/arch/machine.h
        )
    add_custom_target(override_kernel_functions DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel/arch/machine.h)
    add_dependencies(kernel.elf override_kernel_functions)

    # Add trace_kernel_clearMemory.h to the front of the includes list.
    target_compile_options(kernel.elf BEFORE PRIVATE -include ${CMAKE_CURRENT_LIST_DIR}/trace_kernel_clearMemory.h)
    # Add our preprocessed arch/arm/arch/machine.h to the front of the include list too.
    target_include_directories(kernel.elf BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/kernel/")
endif()
