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
#include "rijndael-alg-fst.h"
#include <stddef.h>
#include <stdint.h>

void encrypt(const uint8_t key[AES256_KEY_BITS / 8], const uint8_t iv[AES_BLOCK_SIZE],
        const uint8_t *pt, size_t pt_len, uint8_t *ct) {

    assert(key != NULL);
    assert(iv != NULL);
    assert(pt != NULL);
    assert(ct != NULL);

    /* The plain text is expected to have been padded by the caller */
    assert(pt_len % AES_BLOCK_SIZE == 0);

    /* Generate the key schedule */
    uint32_t rk[4 * (AES256_KEY_ROUNDS + 1)];
    int Nr __attribute__((unused)) = rijndaelKeySetupEnc(rk, key, AES256_KEY_BITS);
    assert(Nr == AES256_KEY_ROUNDS);

    /* The moving vector that we'll XOR into each plain text block as per CBC */
    const uint8_t *vector = iv;

    /* Now do the actual encryption itself block-by-block */
    while (pt_len > 0) {

        uint8_t pt_block[AES_BLOCK_SIZE];
        for (unsigned i = 0; i < AES_BLOCK_SIZE; i++)
            pt_block[i] = pt[i] ^ vector[i];

        rijndaelEncrypt(rk, AES256_KEY_ROUNDS, pt_block, ct);

        vector = ct;
        pt += AES_BLOCK_SIZE;
        ct += AES_BLOCK_SIZE;
        pt_len -= AES_BLOCK_SIZE;
    }
}

void decrypt(const uint8_t key[AES256_KEY_BITS / 8], const uint8_t iv[AES_BLOCK_SIZE],
        const uint8_t *ct, size_t ct_len, uint8_t *pt) {

    assert(key != NULL);
    assert(iv != NULL);
    assert(ct != NULL);
    assert(pt != NULL);

    assert(ct_len % AES_BLOCK_SIZE == 0);

    /* Generate the key schedule */
    uint32_t rk[4 * (AES256_KEY_ROUNDS + 1)];
    int Nr __attribute__((unused)) = rijndaelKeySetupDec(rk, key, AES256_KEY_BITS);
    assert(Nr == AES256_KEY_ROUNDS);

    /* The moving vector that we'll XOR into each plain text block as per CBC */
    const uint8_t *vector = iv;

    /* Now do the actual encryption itself block-by-block */
    while (ct_len > 0) {

        uint8_t pt_block[AES_BLOCK_SIZE];
        rijndaelDecrypt(rk, AES256_KEY_ROUNDS, ct, pt_block);

        for (unsigned i = 0; i < AES_BLOCK_SIZE; i++)
            pt[i] = pt_block[i] ^ vector[i];

        vector = ct;
        pt += AES_BLOCK_SIZE;
        ct += AES_BLOCK_SIZE;
        ct_len -= AES_BLOCK_SIZE;
    }

}

