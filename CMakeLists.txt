#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# dummy change

cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(sel4bench C CXX ASM)

find_package(seL4 REQUIRED)
find_package(elfloader-tool REQUIRED)

# Configuration that applies to all apps
if(NOT Sel4benchAllowSettingsOverride)
    if(KernelArchARM)
        if(KernelPlatformOMAP3 OR KernelPlatformAM335X)
            set(KernelDangerousCodeInjection ON CACHE BOOL "" FORCE)
        else()
            set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)
        endif()
    else()
        set(KernelArmExportPMUUser OFF CACHE BOOL "" FORCE)
    endif()
    if(KernelArchX86)
        set(KernelExportPMCUser ON CACHE BOOL "" FORCE)
        set(KernelX86DangerousMSR ON CACHE BOOL "" FORCE)
        set(AllowUnstableOverhead ON CACHE BOOL "" FORCE)
        set(KernelX86MicroArch "haswell" CACHE STRING "" FORCE)
        set(KernelXSaveFeatureSet 7 CACHE STRING "" FORCE)
        set(KernelXSaveSize 832 CACHE STRING "" FORCE)
    endif()
    set(KernelRootCNodeSizeBits 13 CACHE STRING "" FORCE)
    set(KernelTimeSlice 500 CACHE STRING "" FORCE)
    set(KernelTimerTickMS 1000 CACHE STRING "" FORCE)
    set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)
endif()
sel4_import_kernel()

if((NOT Sel4benchAllowSettingsOverride) AND (KernelArchARM OR KernelArchRiscV))
    # Elfloader settings that correspond to how Data61 sets its boards up.
    ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
endif()
elfloader_import_project()

add_subdirectory(apps/sel4bench)

include(simulation)
GenerateSimulateScript()
