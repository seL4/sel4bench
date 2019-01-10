/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

.text
.balign 32


.macro free_up_x0_and_load_it_with_vector_num _vectorno
    str x0, [sp, #-16]!
    mov x0, \_vectorno
.endm

.macro aligned_vector _vectorsym
    .balign 0x80
    b \_vectorsym
.endm

/* Vector table */
.globl aarch64_el1_vector_table
.balign 0x100
aarch64_el1_vector_table:
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care

    aligned_vector guest_vector_cur_el_sync
    aligned_vector guest_vector_cur_el_irq
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care

    aligned_vector guest_vector_lower_el64_sync
    aligned_vector guest_vector_lower_el64_irq
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care

    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care
    aligned_vector guest_vector_dont_care


.local guest_vector_dont_care
guest_vector_dont_care:
    free_up_x0_and_load_it_with_vector_num #0
    bl guest_vector_common

.local guest_vector_cur_el_sync
guest_vector_cur_el_sync:
    free_up_x0_and_load_it_with_vector_num #4
    bl guest_vector_common

.local guest_vector_cur_el_irq
guest_vector_cur_el_irq:
    free_up_x0_and_load_it_with_vector_num #5
    bl guest_vector_common

.local guest_vector_lower_el64_sync
guest_vector_lower_el64_sync:
    free_up_x0_and_load_it_with_vector_num #8
    bl guest_vector_common

.local guest_vector_lower_el64_irq
guest_vector_lower_el64_irq:
    free_up_x0_and_load_it_with_vector_num #9
    bl guest_vector_common

.globl guest__start
guest__start:
    /* Mask all interrupts off at the local CPU */
    msr daifset, #0xf
    /* Select the SP for the currentEL. */
    msr spsel, #1

    bl guest_main
2:
    wfi
    b 2b
