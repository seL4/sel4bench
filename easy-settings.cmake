#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Define our top level settings.  Whilst they have doc strings for readability
# here, they are hidden in the cmake-gui as they cannot be reliably changed
# after the initial configuration.  Enterprising users can still change them if
# they know what they are doing through advanced mode.
#
# Users should initialize a build directory by doing something like:
#
# mkdir build_sabre
# cd build_sabre
#
# Then
#
# ../griddle --PLATFORM=sabre --SIMULATION
# ninja
#
set(RELEASE ON CACHE BOOL "Performance optimized build")
set(PLATFORM "x86_64" CACHE STRING "Platform to test")
set(FASTPATH ON CACHE BOOL "Turn fastpath on or off")
set(ARM_HYP OFF CACHE BOOL "ARM EL2 hypervisor features on or off")
set(MCS OFF CACHE BOOL "MCS kernel")

# Set the list of benchmark applications to be included into the image
set(HARDWARE OFF CACHE BOOL "Application to benchmark hardware-related operations")
set(FAULT ON CACHE BOOL "Application to benchmark seL4 faults")
set(VCPU OFF CACHE BOOL "Application to benchmark seL4 VCPU performance")
set(SMP OFF CACHE BOOL "Application SMP benchmarks")
set(IPC ON CACHE BOOL "Application to benchmark seL4 IPC")
# see apps/irq/CMakeLists.txt for current state of the benchmark
set(IRQ OFF CACHE BOOL "Application to benchmark seL4 IRQs from inside the kernel")
set(IRQUSER ON CACHE BOOL "Application to benchmark seL4 IRQs")
set(SCHED ON CACHE BOOL "Application to benchmark seL4 scheduler")
set(SIGNAL ON CACHE BOOL "Application to benchmark seL4 signals")
set(MAPPING ON CACHE BOOL "Application to benchmark seL4 mapping a series of pages")
set(SYNC ON CACHE BOOL "Application to benchmark seL4 sync")
