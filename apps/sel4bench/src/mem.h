/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __MEM_H__
#define __MEM_H__

#include <twinkle/object_allocator.h>
#include <vka/vka.h>

/* The maximum number of fragments in our virtual memory allocator. */
#define VMEM_MAX_FRAGMENTS 1000

/* The virtual address of the start of the virtual memory pool. */
#define VMEM_POOL_START 0x40000000

/* The size of the virtual memory pool allocated by the allocator. */
#define VMEM_POOL_SIZE 0x10000000

void*
mem_alloc(vka_t *vka, seL4_Word size, seL4_Word alignment);

void
mem_free(vka_t *vka, void *ptr);

seL4_Word
vmem_alloc(seL4_Word size, seL4_Word alignment);

void
vmem_free(seL4_Word ptr);

seL4_CPtr
frame_alloc(vka_t *vka, seL4_Word size);

void
frame_free(vka_t *vka, seL4_CPtr cap);

#endif /* __MEM_H__ */
