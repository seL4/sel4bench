/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#include <assert.h>
#include "crypto.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

void pad(uint8_t *pt, size_t pt_len) {

    assert(pt != NULL);

    /* Implement PKCS7 (RFC 5652) padding */
    size_t padding_len = AES_BLOCK_SIZE - pt_len % AES_BLOCK_SIZE;
    assert(padding_len <= UINT8_MAX);
    memset(pt + pt_len, (int)padding_len, padding_len);
}

