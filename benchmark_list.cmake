#
# Copyright 2022, Nataliya Korovkina <malus.brandywine@gmail.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#


# Set the list of benchmark applications to be included into the image
# Uncomment command set() when needed


# Application to benchmark seL4 IPC
# default AppIpcBench is ON
# set(AppIpcBench OFF)


# Application to benchmark seL4 IRQs from inside the kernel
# default AppIrqBench is OFF, see apps/irq/CMakeLists.txt for current state of the benchmark
# set(AppIrqBench ON)


# Application to benchmark seL4 IRQs without modification to the kernel
# default AppIrqUserBench is ON
# set(AppIrqUserBench OFF)


# Application to benchmark seL4 scheduler without modification to the kernel
# default AppSchedulerBench is ON
# set(AppSchedulerBench OFF)


# Application to benchmark seL4 signals without modification to the kernel
# default AppSignalBench is ON
# set(AppSignalBench OFF)


# Enable SMP benchmarks
# default AppSmpBench is OFF
# set(AppSmpBench ON)


# Application to benchmark seL4 VCPU performance
# default AppVcpuBench is OFF
# set(AppVcpuBench ON)


# Application to benchmark seL4 mapping a series of pages
# default AppPageMappingBench is ON
# set(AppPageMappingBench OFF)


# Application to benchmark seL4 sync
# default AppSyncBench is ON
# set(AppSyncBench OFF)

# Application to benchmark seL4 faults without modification to the kernel
# default AppFaultBench is OFF
# set(AppFaultBench ON)


# Application to benchmark hardware-related operations
# default AppHardwareBench is OFF
# set(AppHardwareBench ON)
