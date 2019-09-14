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

set(project_dir "${CMAKE_CURRENT_LIST_DIR}/../../")
file(GLOB project_modules ${project_dir}/projects/*)
list(
    APPEND
        CMAKE_MODULE_PATH
        ${project_dir}/kernel
        ${project_dir}/tools/seL4/cmake-tool/helpers/
        ${project_dir}/tools/seL4/elfloader-tool/
        ${project_modules}
)

set(NANOPB_SRC_ROOT_FOLDER "${project_dir}/nanopb" CACHE INTERNAL "")

include(application_settings)

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4benchAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")

include(${CMAKE_CURRENT_LIST_DIR}/easy-settings.cmake)

if(VCPU)
    set(ARM_HYP ON CACHE BOOL "ARM EL2 hypervisor features on or off" FORCE)
endif()
correct_platform_strings()

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()
if(RISCV32 OR RISCV64)
    message(FATAL_ERROR "RISC-V is not supported by sel4bench")
endif()
set(
    valid_platforms ${KernelPlatform_all_strings}
    # The following plaforms are for backwards compatibility reasons.
    sabre
    wandq
    kzm
    rpi3
    exynos5250
    exynos5410
    exynos5422
    am335x-boneblack
    am335x-boneblue
    x86_64
    ia32
)
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
list(FIND valid_platforms "${PLATFORM}" index)
if("${index}" STREQUAL "-1")
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

set(LibNanopb ON CACHE BOOL "" FORCE)

# We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
# in the cmake-gui to prevent excessively confusing users
if(NOT Sel4benchAllowSettingsOverride)
    # Determine the platform/architecture
    if(KernelArchARM)
        if(NOT VCPU)
            set(AppVcpuBench OFF CACHE BOOL "" FORCE)
        endif()
        if(KernelSel4ArchAarch64)
            if(ARM_HYP)
                set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
            endif()
        endif()

        # Elfloader settings that correspond to how Data61 sets its boards up.
        ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
    elseif(KernelArchX86)
        set(AllowUnstableOverhead ON CACHE BOOL "" FORCE)
        set(KernelX86MicroArch "haswell" CACHE STRING "" FORCE)
        set(KernelXSaveFeatureSet 7 CACHE STRING "" FORCE)
        set(KernelXSaveSize 832 CACHE STRING "" FORCE)
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
        if(KernelPlatformKZM OR KernelPlatformOMAP3 OR KernelPlatformAM335X)
            set(KernelDangerousCodeInjection ON CACHE BOOL "" FORCE)
        else()
            set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)
        endif()

        if(KernelPlatformKZM)
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
        if(KernelPlatformKZM)
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
            elseif(KernelPlatformTK1)
                set(ElfloaderMode "monitor" CACHE STRING "" FORCE)
            endif()
        else()
            message(FATAL_ERROR "SMP requires release build (-DRELEASE=TRUE)")
        endif()
    else()
        set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
        set(AppSmpBench OFF CACHE BOOL "" FORCE)
        if(KernelPlatformTK1)
            set(ElfloaderMode "monitor" CACHE STRING "" FORCE)
        endif()
    endif()
endif()
