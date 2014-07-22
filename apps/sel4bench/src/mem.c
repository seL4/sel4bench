/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*
 * This file provides a simple allocator for dividing up a virtual memory
 * region.
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>

#include <vka/vka.h>
#include <vka/object.h>

#include <sel4utils/mapping.h>
#include <sel4utils/util.h>

#include "helpers.h"
#include "mem.h"

#define MEM_DEBUG 0

struct allocation {
    seL4_Word vaddr;
    seL4_Word size;
    vka_object_t frame;
    struct allocation *next;
};

static struct allocation allocation_pool[VMEM_MAX_FRAGMENTS];
static int allocation_pool_used[VMEM_MAX_FRAGMENTS];
static int last_alloc_index;
static struct allocation *virtmem_alloc_list;
static struct allocation *virtmem_free_list;

static struct allocation*
get_allocation_object(void) {
    int i;
    for (i = last_alloc_index + 1; i != last_alloc_index; i++) {
        if (i >= VMEM_MAX_FRAGMENTS) {
            i -= VMEM_MAX_FRAGMENTS;
        }

        if (!allocation_pool_used[i]) {
            last_alloc_index = i;
            allocation_pool_used[i] = 1;
            return &allocation_pool[i];
        }
    }
    /* Callers can't handle failure. */
    assert(0);
    return NULL;
}

static void
free_allocation_object(struct allocation *obj)
{
    int i = obj - allocation_pool;
    assert(i >= 0);
    assert(i < VMEM_MAX_FRAGMENTS);
    allocation_pool_used[i] = 0;
    memset(obj, 0, sizeof(*obj));
}

static void
init_allocators()
{
    static int initialized = 0;
    if (initialized) {
        return;
    }

    struct allocation* alloc = get_allocation_object();
    alloc->vaddr = VMEM_POOL_START;
    alloc->size = VMEM_POOL_SIZE;
    alloc->next = NULL;

    virtmem_free_list = alloc;

    initialized = 1;
}

static void
dump_free_list(void)
{
#if MEM_DEBUG
    struct allocation *alloc;
    printf("Free list:");
    for (alloc = virtmem_free_list; alloc != NULL; alloc = alloc->next) {
        printf(" %x(+0x%x)", alloc->vaddr, alloc->size);
    }
    printf("\n");
#endif
}

static void
dump_alloc_list(void)
{
#if MEM_DEBUG
    struct allocation *alloc;
    printf("Alloc list:");
    for (alloc = virtmem_alloc_list; alloc != NULL; alloc = alloc->next) {
        printf(" %x(+0x%x)", alloc->vaddr, alloc->size);
    }
    printf("\n");
#endif
}

static struct allocation*
virtmem_allocate(seL4_Word size, seL4_Word alignment) {
    init_allocators();

    dump_free_list();

    assert(IS_POWER_OF_2(alignment));

    assert(size > 0);

    struct allocation *alloc;
    struct allocation *prev_alloc = NULL;
    seL4_Word vaddr = 0;
    for (alloc = virtmem_free_list; alloc != NULL; alloc = alloc->next) {
        /* Align start of section up and see if there is still space. */
        vaddr = ALIGN_UP(alloc->vaddr, alignment);

        /*
         * Ensure we don't overflow, and ensure this allocation is big
         * enough.
         */
        if (vaddr + size >= vaddr &&
                vaddr - alloc->vaddr <= alloc->size &&
                alloc->size - (vaddr - alloc->vaddr) >= size) {
            /* We have a winner! */
            break;
        }
    }

    if (alloc == NULL) {
        return NULL;
    }

    /*
     * Four cases:
     *  1. Allocation is a perfect fit.
     *  2. Allocation leaves an extra piece before.
     *  3. Allocation leaves an extra piece after.
     *  4. Allocation leaves an extra pieces before and after.
     */
    if (vaddr != alloc->vaddr) {
        /* There is space before us. Is there space after? */
        if (vaddr + size != alloc->vaddr + alloc->size) {
            /* Space after. Make a new allocation for it. */
            struct allocation *new_alloc = get_allocation_object();
            new_alloc->vaddr = vaddr + size;
            new_alloc->size = (alloc->vaddr + alloc->size) - (vaddr + size);
            new_alloc->next = alloc->next;
            alloc->next = new_alloc;
            assert(new_alloc->size > 0);
        }
        /*
         * Trim the existing allocation to contain precisely the space
         * before the new allocation.
         */
        alloc->size = vaddr - alloc->vaddr;
        assert(alloc->size > 0);
    } else {
        /* No space before. Should we use this alloc for the space after? */
        if (size != alloc->size) {
            alloc->vaddr = vaddr + size;
            alloc->size -= size;
        } else {
            /* Perfect fit. Prune this entry from the free list. */
            if (prev_alloc == NULL) {
                virtmem_free_list = alloc->next;
            } else {
                prev_alloc->next = alloc->next;
            }
            free_allocation_object(alloc);
        }
    }

    /* Add to allocated list. */
    struct allocation* new_alloc = get_allocation_object();
    new_alloc->vaddr = vaddr;
    new_alloc->size = size;
    new_alloc->next = virtmem_alloc_list;
    virtmem_alloc_list = new_alloc;

    dump_free_list();
    dump_alloc_list();

    return new_alloc;
}

static struct allocation *
virtmem_get_allocation(void *ptr) {
    /* Find the allocation given the pointer.
     * Only the base of each allocation is recognised. A pointer in the middle
     * of an allocation is not considered, so don't do that. */
    init_allocators();

    seL4_Word vaddr = (seL4_Word)ptr;
    assert(vaddr != 0);

    /* Locate ptr in allocation list. */
    struct allocation *this_alloc = NULL;
    for (this_alloc = virtmem_alloc_list;
            this_alloc != NULL;
            this_alloc = this_alloc->next) {
        if (this_alloc->vaddr == vaddr) {
            return this_alloc;
        }
    }
    return NULL;
}

static void
virtmem_free(void *ptr)
{
    init_allocators();

    seL4_Word vaddr = (seL4_Word)ptr;

    assert(vaddr != 0);

    dump_alloc_list();
    dump_free_list();
    /* Locate ptr in allocation list. */
    struct allocation *this_alloc, *prev_alloc = NULL;
    for (this_alloc = virtmem_alloc_list;
            this_alloc != NULL;
            this_alloc = this_alloc->next) {
        if (this_alloc->vaddr == vaddr) {
            break;
        }
        prev_alloc = this_alloc;
    }
    assert(this_alloc != NULL);

    if (prev_alloc == NULL) {
        virtmem_alloc_list = this_alloc->next;
    } else {
        prev_alloc->next = this_alloc->next;
    }

    /*
     * If we come before the first block in the free list, just drop it in
     * and we're done.
     */
    if (virtmem_free_list == NULL || virtmem_free_list->vaddr > vaddr) {
        this_alloc->next = virtmem_free_list;
        virtmem_free_list = this_alloc;
    } else {
        /*
         * Otherwise, find the free block before its final position in the
         * list. We are guaranteed that there is one.
         */
        struct allocation *prev_free_alloc;
        for (prev_free_alloc = virtmem_free_list;
                prev_free_alloc != NULL;
                prev_free_alloc = prev_free_alloc->next) {
            if (prev_free_alloc->next == NULL ||
                    prev_free_alloc->next->vaddr > vaddr) {
                /* Bingo. */
                break;
            }
        }

        /*
         * We should always find a block, otherwise we would have taken the
         * first half of the enclosing if statement.
         */
        assert(prev_free_alloc != NULL);

        /* Stick it in between prev_free_alloc and it's successor. */
        this_alloc->next = prev_free_alloc->next;
        prev_free_alloc->next = this_alloc;

        /* Should we merge prev_free_alloc with this_alloc? */
        if (prev_free_alloc->vaddr + prev_free_alloc->size == this_alloc->vaddr) {
            prev_free_alloc->size += this_alloc->size;
            prev_free_alloc->next = this_alloc->next;
            free_allocation_object(this_alloc);
            this_alloc = prev_free_alloc;
        }
    }

    /* Should we merge this allocation with the next? */
    if (this_alloc->next &&
            this_alloc->vaddr + this_alloc->size == this_alloc->next->vaddr) {
        struct allocation *subsumed_alloc = this_alloc->next;
        this_alloc->size += this_alloc->next->size;
        this_alloc->next = this_alloc->next->next;
        free_allocation_object(subsumed_alloc);
    }

    dump_free_list();
}

void*
mem_alloc(vka_t *vka, seL4_Word size, seL4_Word alignment)
{
    /* For small allocations, just use malloc(). */
    if (size < 4096) {
        /*
         * Allocate enough space for the request, and any wasted alignment fu.
         * We store the original pointer used for allocation 4 bytes before the
         * start of the returned aligned pointer. This allows us to pass that
         * directly to free().
         */
        void *ptr = malloc(size + sizeof(void*) + alignment - 1);
        assert(ptr != NULL);

        void *ret = (void*)ALIGN_UP(sizeof(void*) + (seL4_Word)ptr, alignment);
        void **p = (void**)ret;

        /* I haven't yet convinced myself that these holds true -- brb */
        assert(&p[-1] >= (void**)ptr);
        assert(ret + size < ptr + size + sizeof(void*) + alignment - 1);

        p[-1] = ptr;
        return ret;
    }

    /* For larger allocations, find the largest frame that can fit it. This can
     * be horridly inefficient. Brrrr. */
#ifdef ARCH_ARM
    if (size <= (1 << 12)) {
        size = 1 << 12;
    } else if (size <= (1 << 16)) {
        size = 1 << 16;
    } else if (size <= (1 << 20)) {
        size = 1 << 20;
    } else if (size <= (1 << 24)) {
        size = 1 << 24;
    } else {
        assert(0);
    }
#else
    if (size <= (1 << 12)) {
        size = 1 << 12;
    } else if (size <= (1 << 22)) {
        size = 1 << 22;
    } else {
        assert(0);
    }
#endif

    if (alignment < size) {
        alignment = size;
    }

    struct allocation *alloc = virtmem_allocate(size, alignment);


    int error = vka_alloc_frame(vka, CTZ(size), &alloc->frame);
    (void)error;
    assert(!error);

    sel4utils_map_page_leaky(vka, seL4_CapInitThreadPD, alloc->frame.cptr, (void*)alloc->vaddr, seL4_AllRights, true);

    return (void*)alloc->vaddr;
}

void
mem_free(vka_t *vka, void *ptr)
{
    struct allocation *vallocation = virtmem_get_allocation(ptr);
    if (vallocation != NULL) {
        seL4_ARCH_Page_Unmap(vallocation->frame.cptr);
        virtmem_free(ptr);
        vka_free_object(vka, &vallocation->frame);
    } else {
        void **p = (void**)ptr;
        free(p[-1]);
    }
}

seL4_Word
vmem_alloc(seL4_Word size, seL4_Word alignment)
{
    struct allocation *alloc = virtmem_allocate(size, alignment);

    if (alloc != NULL) {
        return alloc->vaddr;
    } else {
        return 0;
    }
}

void
vmem_free(seL4_Word ptr)
{
    virtmem_free((void*)ptr);
}
