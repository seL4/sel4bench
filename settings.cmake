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

set(SEL4_CONFIG_DEFAULT_ADVANCED ON)
include(application_settings)

include(${CMAKE_CURRENT_LIST_DIR}/easy-settings.cmake)

if(VCPU)
    set(ARM_HYP ON CACHE BOOL "ARM EL2 hypervisor features on or off" FORCE)
endif()
correct_platform_strings()

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()

if(KernelArchRiscV)
    message(FATAL_ERROR "RISC-V is not supported by sel4bench")
endif()
set(valid_platforms ${KernelPlatform_all_strings} ${correct_platform_strings_platform_aliases})
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
if(NOT "${PLATFORM}" IN_LIST valid_platforms)
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4benchAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")
if(NOT Sel4benchAllowSettingsOverride)
    # We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
    # in the cmake-gui to prevent excessively confusing users
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

    endif()

    # Setup flags for RELEASE (performance optimized builds)
    ApplyCommonReleaseVerificationSettings(${RELEASE} OFF)
    # This option is controlled by ApplyCommonReleaseVerificationSettings
    mark_as_advanced(CMAKE_BUILD_TYPE)
    if(RELEASE)
        set(KernelFWholeProgram ON CACHE BOOL "" FORCE)
    endif()

    if(FASTPATH)
        set(KernelFastpath ON CACHE BOOL "" FORCE)
    else()
        set(KernelFastpath OFF CACHE BOOL "" FORCE)
    endif()

    # App-specific configuration
    if(FAULT)
        set(AppFaultBench ON CACHE BOOL "" FORCE)
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
