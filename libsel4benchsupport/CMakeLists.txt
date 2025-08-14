#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#
cmake_minimum_required(VERSION 3.16.0)

project(libsel4benchsupport C)

file(GLOB deps src/*.c src/arch/${KernelArch}/*.c)

list(SORT deps)

add_library(sel4benchsupport EXCLUDE_FROM_ALL ${deps})
target_include_directories(
    sel4benchsupport PUBLIC "include/" "arch_include/${KernelArch}/"
                            "sel4_arch_include/${KernelSel4Arch}/"
)
target_link_libraries(
    sel4benchsupport
    sel4_autoconf
    muslc
    sel4runtime
    sel4allocman
    sel4bench
    sel4muslcsys
    sel4rpc
    sel4serialserver
)
