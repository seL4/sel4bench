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

project(ipc C)

set(configure_string "")
config_option(
    AppIpcBench
    APP_IPCBENCH
    "Application to benchmark seL4 IPC."
    DEFAULT
    ON
    DEPENDS
    "DefaultBenchDeps"
)
config_choice(
    CounterToMeasure
    COUNTER_TO_MEASURE
    "Counter to measure"
    "Cycle count;Ipc_CycleCount;CYCLE_COUNT;AppIpcBench"
    "Generic counter;Ipc_GenericCounter;GENERIC_COUNTER;AppIpcBench"
    "Platform counter;Ipc_PlatformCounter;PLATFORM_COUNTER;AppIpcBench"
)
config_choice(
    GenericCounterID
    GENERIC_COUNTER_ID
    "Select the generic counter to measure in ipc benchmarks"
    "0;Ipc_CacheL1IMiss;CACHE_L1I_MISS;Ipc_GenericCounter;AppIpcBench"
    "1;Ipc_CacheL1DMiss;CACHE_L1D_MISS;Ipc_GenericCounter;AppIpcBench"
    "2;Ipc_TLBL1IMiss;TLB_L1I_MISS;Ipc_GenericCounter;AppIpcBench"
    "3;Ipc_TLBL1DMiss;TLB_L1D_MISS;Ipc_GenericCounter;AppIpcBench"
    "4;Ipc_ExecuteInstruction;EXECUTE_INSTRUCTION;Ipc_GenericCounter;AppIpcBench"
    "5;Ipc_BranchMispredict;BRANCH_MISPREDICT;Ipc_GenericCounter;AppIpcBench"
    "6;Ipc_MemoryAccess;MEMORY_ACCESS;Ipc_GenericCounter;AppIpcBench"
)
config_string(
    PlatformCounterConstant
    PLATFORM_COUNTER_CONSTANT
    "Internal constant to use for the event"
    DEFAULT
    SEL4BENCH_EVENT_CYCLE_CORE
    DEPENDS
    "Ipc_PlatformCounter"
    UNDEF_DISABLED
    UNQUOTE
)
add_config_library(sel4benchipc "${configure_string}")

file(GLOB deps src/*.c)
list(SORT deps)
add_executable(ipc EXCLUDE_FROM_ALL ${deps})
target_link_libraries(
    ipc
    sel4_autoconf
    sel4benchipc_Config
    sel4
    muslc
    cpio
    sel4vka
    sel4vspace
    sel4allocman
    sel4utils
    elf
    sel4simple
    sel4benchsupport
    sel4debug
    platsupport
    sel4platsupport
    sel4muslcsys
)

if(AppIpcBench)
    set_property(GLOBAL APPEND PROPERTY sel4benchapps_property "$<TARGET_FILE:ipc>")
endif()
