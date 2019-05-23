#
# Copyright 2019, Data61
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
# Default dependencies on kernel benchmarking features. Declared here so that
# all the benchmark applications can use it
if(
    (KernelArchX86 AND KernelExportPMCUser AND KernelX86DangerousMSR)
    OR (KernelArchARM AND KernelArmExportPMUUser)
    OR (KernelArchArmV6 AND KernelDangerousCodeInjection)
    OR (KernelArchArmCortexA8 AND KernelDangerousCodeInjection)
)
    set(DefaultBenchDeps TRUE)
else()
    set(DefaultBenchDeps FALSE)
endif()

add_subdirectory(apps/fault)
add_subdirectory(apps/hardware)
add_subdirectory(apps/ipc)
add_subdirectory(apps/irq)
add_subdirectory(apps/irquser)
add_subdirectory(apps/page_mapping)
add_subdirectory(apps/scheduler)
add_subdirectory(apps/signal)
add_subdirectory(apps/smp)
add_subdirectory(apps/sync)
add_subdirectory(apps/vcpu)
add_subdirectory(libsel4benchsupport)

# This needs to be after the applications so that sel4bench can grab all the elf files.
add_subdirectory(apps/sel4bench)
link_libraries(kernel_autoconf)

GenerateSimulateScript()
