/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <twinkle/allocator.h>
#include "allocator_helpers.h"

void setup_child_allocator(struct allocator *parent, struct allocator *child) {
    allocator_create_child(parent, child,
            parent->root_cnode,
            parent->root_cnode_depth,
            parent->root_cnode_offset,
            parent->cslots.first + parent->num_slots_used,
            parent->cslots.count - parent->num_slots_used);
}
