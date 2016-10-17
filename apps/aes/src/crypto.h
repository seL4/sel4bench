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
/* Common functions */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Some AES-related constants */
enum {
    AES_BLOCK_SIZE = 16,
    AES256_KEY_BITS = 256,
    AES256_KEY_ROUNDS = 14,
};

/** Pad some plain text
 *
 * The available memory at `pt` is expected to contain room for the padding which is done in-place.
 * The amount of space required is `pt_len + AES_BLOCK_SIZE - pt_len % AES_BLOCK_SIZE`.
 *
 * @param pt The plain text data to pad
 * @param pt_len The length of the data at pt
 */
void pad(uint8_t *pt, size_t pt_len);

/** Encrypt some plain text
 *
 * The plain text must by 16-byte aligned (i.e. already padded). `ct` is expected to point to
 * `pt_len` bytes of accessible memory.
 *
 * @param key A 256-bit key
 * @param iv A 128-bit initialisation vector
 * @param pt The plain text to encrypt
 * @param pt_len The length of the plain text
 * @param[out] ct The encrypted cipher text
 */
void encrypt(const uint8_t key[AES256_KEY_BITS / 8], const uint8_t iv[AES_BLOCK_SIZE],
        const uint8_t *pt, size_t pt_len, uint8_t *ct);

/** Decrypt some cipher text
 *
 * The cipher text is expected to be 16-byte aligned. `pt` is expected to point to `ct_len` bytes
 * of accessible memory.
 *
 * @param key The 256-bit key used during encryption
 * @param iv The 128-bit initialisation vector used during encryption
 * @param ct The cipher text
 * @param ct_len The length of the cipher text
 * @param[out] pt The decrypted plain text
 */
void decrypt(const uint8_t key[AES256_KEY_BITS / 8], const uint8_t iv[AES_BLOCK_SIZE],
        const uint8_t *ct, size_t ct_len, uint8_t *pt);

