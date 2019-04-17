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
set(HARDWARE OFF CACHE BOOL "Configuration for sel4bench hardware app")
set(FAULT OFF CACHE BOOL "Configuration sel4bench fault app")
set(VCPU OFF CACHE BOOL "Whether or not to run the VCPU benchmarks")
set(SMP OFF CACHE BOOL "Configuration sel4bench smp app")
set(PLATFORM "x86_64" CACHE STRING "Platform to test")
set(FASTPATH ON CACHE BOOL "Turn fastpath on or off")
set(ARM_HYP OFF CACHE BOOL "ARM EL2 hypervisor features on or off")
