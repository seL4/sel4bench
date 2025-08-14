#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

set(SEL4_BENCH_SUPPORT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/libsel4benchsupport"
    CACHE STRING ""
)
mark_as_advanced(SEL4_BENCH_SUPPORT_DIR)

macro(sel4benchsupport_import_libraries)
    add_subdirectory(${SEL4_BENCH_SUPPORT_DIR} sel4benchsupport)
endmacro()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(sel4benchsupport DEFAULT_MSG SEL4_BENCH_SUPPORT_DIR)
