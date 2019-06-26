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

include(${KERNEL_HELPERS_PATH})

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4benchAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")

include(easy-settings.cmake)

# set_property(CACHE PLATFORM PROPERTY STRINGS "x86_64;ia32;sabre;jetson;arndale;bbone;bbone_black;beagle;hikey;hikey64;inforce;kzm;odroidx;odroidxu;rpi3;zynq")
set_property(
    CACHE PLATFORM
    PROPERTY STRINGS ${KernelX86Sel4Arch_all_strings} ${KernelARMPlatform_all_strings}
)

# We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
# in the cmake-gui to prevent excessively confusing users
if(NOT Sel4benchAllowSettingsOverride)
    # Determine the platform/architecture
    if(${PLATFORM} IN_LIST KernelX86Sel4Arch_all_strings)
        set(KernelArch x86 CACHE STRING "" FORCE)
        set(KernelX86Sel4Arch ${PLATFORM} CACHE STRING "" FORCE)
        set(AllowUnstableOverhead ON CACHE BOOL "" FORCE)
        set(KernelX86MicroArch "haswell" CACHE STRING "" FORCE)
        set(KernelXSaveFeatureSet 7 CACHE STRING "" FORCE)
        set(KernelXSaveSize 832 CACHE STRING "" FORCE)
    else()
        if(NOT ${PLATFORM} IN_LIST KernelARMPlatform_all_strings)
            message(FATAL_ERROR "Unknown PLATFORM. Initial configuration may not work")
        endif()
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelARMPlatform ${PLATFORM} CACHE STRING "" FORCE)

        if(VCPU)
            set(ARM_HYP ON CACHE BOOL "ARM EL2 hypervisor features on or off" FORCE)
        else()
            set(AppVcpuBench OFF CACHE BOOL "" FORCE)
        endif()

        if(AARCH64)
            set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
            if(ARM_HYP)
                set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
            endif()
        else()
            if(ARM_HYP)
                set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
                set(KernelArmSel4Arch "arm_hyp" CACHE STRING "" FORCE)
            else()
                set(KernelArmSel4Arch "aarch32" CACHE STRING "" FORCE)
            endif()
        endif()
        # Elfloader settings that correspond to how Data61 sets its boards up.
        ApplyData61ElfLoaderSettings(${KernelARMPlatform} ${KernelArmSel4Arch})
    endif()

    # Setup flags for RELEASE (performance optimized builds)
    if(RELEASE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
        set(KernelFWholeProgram ON CACHE BOOL "" FORCE)
        set(KernelDebugBuild OFF CACHE BOOL "" FORCE)
        set(KernelPrinting OFF CACHE BOOL "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
        set(KernelDebugBuild ON CACHE BOOL "" FORCE)
        set(KernelPrinting ON CACHE BOOL "" FORCE)
    endif()

    if(FASTPATH)
        set(KernelFastpath ON CACHE BOOL "" FORCE)
    else()
        set(KernelFastpath OFF CACHE BOOL "" FORCE)
    endif()
    # Configuration that applies to all apps
    if(KernelArchARM)
        if(KernelArmCortexA8 OR KernelArchArmV6)
            set(KernelDangerousCodeInjection ON CACHE BOOL "" FORCE)
        else()
            set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)
        endif()

        if(KernelArchArmV6)
            set(KernelDangerousCodeInjectionOnUndefInstr ON CACHE BOOL "" FORCE)
        endif()
    else()
        set(KernelArmExportPMUUser OFF CACHE BOOL "" FORCE)
    endif()
    if(KernelArchX86)
        set(KernelExportPMCUser ON CACHE BOOL "" FORCE)
        set(KernelX86DangerousMSR ON CACHE BOOL "" FORCE)
    endif()
    set(KernelRootCNodeSizeBits 13 CACHE STRING "" FORCE)
    set(KernelTimeSlice 500 CACHE STRING "" FORCE)
    set(KernelTimerTickMS 1000 CACHE STRING "" FORCE)
    set(LibSel4MuslcSysMorecoreBytes 0 CACHE STRING "" FORCE)
    set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)

    # App-specific configuration
    if(FAULT)
        if("${PLATFORM}" STREQUAL "kzm")
            message(FATAL_ERROR "Fault app not supported on kzm.")
        else()
            set(AppFaultBench ON CACHE BOOL "" FORCE)
        endif()
    else()
        set(AppFaultBench OFF CACHE BOOL "" FORCE)
    endif()

    if(HARDWARE)
        set(KernelBenchmarks "generic" CACHE STRING "" FORCE)
        set(AppHardwareBench ON CACHE BOOL "" FORCE)
    elseif(VCPU)
        # The VCPU benchmarks require the kernel entry and exit timestamp feature.
        set(AppVcpuBench ON CACHE BOOL "" FORCE)
        set(KernelBenchmarks "track_kernel_entries" CACHE STRING "" FORCE)
    else()
        set(KernelBenchmarks "none" CACHE STRING "" FORCE)
        set(AppHardwareBench OFF CACHE BOOL "" FORCE)
    endif()

    if(SMP)
        if(RELEASE)
            set(KernelMaxNumNodes 4 CACHE STRING "" FORCE)
            set(AppSmpBench ON CACHE BOOL "" FORCE)
            if(KernelPlatImx6)
                set(ElfloaderMode "secure supervisor" CACHE STRING "" FORCE)
            elseif(KernelPlattformJetson)
                set(ElfloaderMode "monitor" CACHE STRING "" FORCE)
            endif()
        else()
            message(FATAL_ERROR "SMP requires release build (-DRELEASE=TRUE)")
        endif()
    else()
        set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
        set(AppSmpBench OFF CACHE BOOL "" FORCE)
        if(KernelPlatformJetson)
            set(ElfloaderMode "monitor" CACHE STRING "" FORCE)
        endif()
    endif()
endif()
