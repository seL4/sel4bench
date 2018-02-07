#
# Copyright 2017, Data61
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

# Default dependencies on kernel benchmarking features. Declared here so that
# all the benchmark applications can use it
if( (KernelArchX86 AND KernelExportPMCUser AND KernelX86DangerousMSR) OR
    (KernelArchARM AND KernelArmExportPMUUser) OR
    (KernelArchArmV6 AND KernelDangerousCodeInjection) OR
    (KernelArchArmCortexA8 AND KernelDangerousCodeInjection) )
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
add_subdirectory(libsel4benchsupport)

# This needs to be after the applications so that sel4bench can grab all the elf files.
add_subdirectory(apps/sel4bench)

GenerateSimulateScript()

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4benchAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")

# Define our top level settings. Whilst they have doc strings for readability here
# they are hidden the cmake-gui as they cannot be reliably changed after the initial
# configuration, but enterprising users can still change them if they know what they
# are doing through advanced mode
# Users should initialize a build directory by doing something like
# mkdir build_sabre
# cd build_sabre
# ../init-build.sh -DAARCH32=TRUE -DPLATFORM=sabre -DHARDWARE=TRUE
# You need -DAARCH32=TRUE if you need to use the (32-bit) ARM cross-compiler (see kernel cmake
# files for more info).
set(RELEASE ON CACHE BOOL "Performance optimized build")
set(VERIFICATION OFF CACHE BOOL "Only verification friendly kernel features")
set(HARDWARE OFF CACHE BOOL "Configuration for sel4bench hardware app")
set(FAULT OFF CACHE BOOL "Configuration sel4bench fault app")
set(SMP OFF CACHE BOOL "Configuration sel4bench smp app")
set(PLATFORM "x86_64" CACHE STRING "Platform to test")
set_property(CACHE PLATFORM PROPERTY STRINGS "x86_64;ia32;sabre;jetson;arndale;bbone;bbone_black;beagle;hikey;hikey64;inforce;kzm;odroidx;odroidxu;rpi3;zynq")
mark_as_advanced(RELEASE VERIFICATION HARDWARE FAULT SMP PLATFORM)

# We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
# in the cmake-gui to prevent excessively confusing users
if (NOT Sel4benchAllowSettingsOverride)
    # Determine the platform/architecture
    if ((${PLATFORM} STREQUAL "x86_64") OR (${PLATFORM} STREQUAL "ia32"))
        set(KernelArch x86 CACHE STRING "" FORCE)
        set(KernelX86Sel4Arch ${PLATFORM} CACHE STRING "" FORCE)
        set(AllowUnstableOverhead ON CACHE BOOL "" FORCE)
    elseif(
            (${PLATFORM} STREQUAL "sabre")
            OR (${PLATFORM} STREQUAL "jetson")
            OR (${PLATFORM} STREQUAL "hikey")
            OR (${PLATFORM} STREQUAL "kzm")
            OR (${PLATFORM} STREQUAL "rpi3")
            OR (${PLATFORM} STREQUAL "arndale")
            OR (${PLATFORM} STREQUAL "bbone")
            OR (${PLATFORM} STREQUAL "bbone_black")
            OR (${PLATFORM} STREQUAL "beagle")
            OR (${PLATFORM} STREQUAL "inforce")
            OR (${PLATFORM} STREQUAL "odroid")
            OR (${PLATFORM} STREQUAL "odroidxu")
            OR (${PLATFORM} STREQUAL "zynq")
            )
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch32" CACHE STRING "" FORCE)
        # Some ugly name (un)mangling
        if (${PLATFORM} STREQUAL "arndale")
            set(KernelARMPlatform exynos5250 CACHE STRING "" FROCE)
        elseif((${PLATFORM} STREQUAL "bbone_black") OR (${PLATFORM} STREQUAL "bbone"))
            set(KernelARMPlatform am335x CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "beagle")
            set(KernelARMPlatform omap3 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "inforce")
            set(KernelARMPlatform apq8064 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "odroid")
            set(KernelARMPlatform exynos4 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "odroidxu")
            set(KernelARMPlatform exynos5410 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "zynq")
            set(KernelARMPlatform zynq7000 CACHE STRING "" FORCE)
        else()
            set(KernelARMPlatform ${PLATFORM} CACHE STRING "" FORCE)
        endif()
        if ((${PLATFORM} STREQUAL "hikey") OR (${PLATFORM} STREQUAL "bbone_black"))
            set(ElfloaderImage "binary" CACHE STRING "" FORCE)
        endif()
    elseif(${PLATFORM} STREQUAL "tx1")
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
        set(KernelARMPlatform ${PLATFORM} CACHE STRING "" FORCE)
        set(ElfloaderImage "binary" CACHE STRING "" FORCE)
    elseif(${PLATFORM} STREQUAL "hikey64")
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
        set(KernelARMPlatform hikey CACHE STRING "" FORCE)
        set(ElfloaderImage "efi" CACHE STRING "" FORCE)
    else()
        message(FATAL_ERROR "Unknown PLATFORM. Initial configuration may not work")
    endif()

    # Setup flags for different combinations of RELEASE (performance optimized builds) and
    # VERIFICATION (verification friendly features) builds
    if (RELEASE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    endif()
    if (VERIFICATION)
        set(KernelVerificationBuild ON CACHE BOOL "" FORCE)
    else()
        set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)
    endif()
    # If neither release nor verification then enable debug facilities, otherwise turn them off
    if ((NOT RELEASE) AND (NOT VERIFICATION))
        set(KernelDebugBuild ON CACHE BOOL "" FORCE)
        set(KernelPrinting ON CACHE BOOL "" FORCE)
    else()
        set(KernelDebugBuild OFF CACHE BOOL "" FORCE)
        set(KernelPrinting OFF CACHE BOOL "" FORCE)
    endif()

    # Configuration that applies to all apps
    if (KernelArchARM)
        if (KernelArmCortexA8 OR KernelArchArmV6)
            set(KernelDangerousCodeInjection ON CACHE BOOL "" FORCE)
        else()
            set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)
        endif()

        if (KernelArchArmV6)
            set(KernelDangerousCodeInjectionOnUndefInstr ON CACHE BOOL "" FORCE)
        endif()
    else()
        set(KernelArmExportPMUUser OFF CACHE BOOL "" FORCE)
    endif()
    if (KernelArchX86)
        set(KernelExportPMCUser ON CACHE BOOL "" FORCE)
        set(KernelX86DangerousMSR ON CACHE BOOL "" FORCE)
    endif()
    set(KernelRootCNodeSizeBits 13 CACHE STRING "" FORCE)
    set(LibSel4MuslcSysMorecoreBytes 0 CACHE STRING "" FORCE)

    # App-specific configuration
    if (FAULT)
        if ("${PLATFORM}" STREQUAL "kzm")
            message(FATAL_ERROR "Fault app not supported on kzm.")
        else()
            set(AppFaultBench ON CACHE BOOL "" FORCE)
        endif()
    else()
        set(AppFaultBench OFF CACHE BOOL "" FORCE)
    endif()

    if (HARDWARE)
        if (NOT VERIFICATION)
            set(KernelBenchmarks "generic" CACHE STRING "" FORCE)
            set(AppHardwareBench ON CACHE BOOL "" FORCE)
        else()
            message(FATAL_ERROR "Hardware requires verification build to be off (-DVERIFICATION=FALSE)")
        endif()
    else()
        set(KernelBenchmarks "none" CACHE STRING "" FORCE)
        set(AppHardwareBench OFF CACHE BOOL "" FORCE)
    endif()

    if (SMP)
        if (RELEASE)
            set(KernelMaxNumNodes 4 CACHE STRING "" FORCE)
            set(AppSmpBench ON CACHE BOOL "" FORCE)
            if (KernelPlatImx6)
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
        if (KernelPlatformJetson)
            set(ElfloaderMode "monitor" CACHE STRING "" FORCE)
        endif()
    endif()
endif()
