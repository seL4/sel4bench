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
set(PLATFORM "odroidc2" CACHE STRING "Platform to test")
set(FASTPATH ON CACHE BOOL "Turn fastpath on or off")
set(ARM_HYP OFF CACHE BOOL "ARM EL2 hypervisor features on or off")
set(MCS OFF CACHE BOOL "MCS kernel")

# Set the list of benchmark applications to be included into the image
# Add any new benchmark applications to this list

# default is OFF
set(HARDWARE OFF CACHE BOOL "Application to benchmark hardware-related operations")

# default is OFF
set(FAULT OFF CACHE BOOL "Application to benchmark seL4 faults")

# default is OFF
set(VCPU OFF CACHE BOOL "Application to benchmark seL4 VCPU performance")

# default is OFF
set(SMP OFF CACHE BOOL "Application SMP benchmarks")

# default is ON
set(IPC ON CACHE BOOL "Application to benchmark seL4 IPC")

# see apps/irq/CMakeLists.txt for current state of the benchmark
# default is OFF
set(IRQ OFF CACHE BOOL "Application to benchmark seL4 IRQs from inside the kernel")

# default is ON
set(IRQUSER ON CACHE BOOL "Application to benchmark seL4 IRQs")

# default is ON
set(SCHED ON CACHE BOOL "Application to benchmark seL4 scheduler")

# default is ON
set(SIGNAL ON CACHE BOOL "Application to benchmark seL4 signals")

# default is ON
set(MAPPING ON CACHE BOOL "Application to benchmark seL4 mapping a series of pages")

# default is ON
set(SYNC ON CACHE BOOL "Application to benchmark seL4 sync")

# Allow Early Processing methodology for
#Signal/"Signal to High Prio Thread" benchmark
#set(AppSignalEarlyProcessing ON)
