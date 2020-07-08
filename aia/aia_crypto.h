/*
 * Copyright (C) 2019 - 2020 Arm Ltd.  All Rights Reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _AIA_CRYPTO_H_
#define _AIA_CRYPTO_H_

#include <stdint.h>

#include "mbedtls/base64.h"
#include "mbedtls/cipher.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"

typedef enum
{
    eCryptoSuccess = 0,
    eCryptoFailure = -1,
    eCryptoSequenceNotMatch = -2,
} AIACryptoErrorCode_t;

typedef struct {
    /* entropy pool for seeding PRNG */
    mbedtls_entropy_context entropy;

    /* pseudo-random generator */
    mbedtls_ctr_drbg_context drbg;
} AIACrypto_t;

typedef struct {
    const char *client_public_key;
    const char *client_private_key;
    const char *peer_public_key;
} AIACryptoKeys_t;

/**
 * @brief                   The AIA message encryption function using AES-GCM.
 *
 * @param[in] crypto        AIACrypto_t structure containing crypto info.
 * @param[out] msg_buf      The buffer holding the encrypted AIA message to be sent.
 * @param[in] plaintext     The original data. Note that the memory pointed to by this
 *                          pointer must contain enough space preceding it for the
 *                          sequence number.
 * @param[in] plaintext_len The length of the original data.
 * @param[in] sequence      The sequence number of this message.
 *
 * @return                  The length of the message to be sent on success. Negative
 *                          value on failure.
 */
int32_t AIA_CRYPTO_Encrypt( AIACrypto_t * crypto, void * msg_buf, void * plaintext, uint32_t plaintext_len, uint32_t sequence );

/**
 * @brief                       The AIA message decryption function using AES-GCM.
 *
 * @param[in] crypto            AIACrypto_t structure containing crypto info.
 * @param[out] msg_buf          The buffer holding the decrypted blob which contains
 *                              the sequence number and the actual message content.
 * @param[in] encrypted_msg     The encrypted message received from the service.
 * @param[in] encrypted_msg_len The length of the encrypted message.
 *
 * @return                      The length of the decrypted message content on success.
 *                              Negative value on failure including the length of the
 *                              decrypted sequence number!
 */
int32_t AIA_CRYPTO_Decrypt( AIACrypto_t * crypto, void * msg_buf, const void * encrypted_msg, uint32_t encrypted_msg_len );

/**
 * @brief                       The initialization function for AIA crypto context.
 *
 * @param[in] crypto            AIACrypto_t structure containing crypto info.
 * @param[in] keys              Keys including self public/private keys and server public key.
 *
 * @return                      Status of initialization.
 */
AIACryptoErrorCode_t AIA_CRYPTO_Init( AIACrypto_t * crypto, AIACryptoKeys_t * keys );

/**
 * @brief                       The function to destroy crypto context.
 *
 * @param[in] crypto            AIACrypto_t structure containing crypto info.
 */
void AIA_CRYPTO_Destory( AIACrypto_t * crypto );

#endif /* _AIA_CRYPTO_H_ */
