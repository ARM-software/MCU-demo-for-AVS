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

#include <string.h>

#include "aia_crypto.h"
#include "aia_client.h"

#define KEY_SIZE    ( 32 )

static uint8_t shared_secret[ KEY_SIZE ];

static void prvReverseArray( uint8_t * arr, size_t len )
{
    for( int i = 0; i < len / 2; i++ )
    {
        uint8_t tmp = arr[ i ];
        arr[ i ] = arr[ len - 1 - i ];
        arr[ len - 1 - i ] = tmp;
    }
}

static AIACryptoErrorCode_t prvGenerateSharedSecret( AIACryptoKeys_t *keys )
{
    AIACryptoErrorCode_t ret;
    uint8_t cli_public_key[ KEY_SIZE ];
    uint8_t cli_private_key[ KEY_SIZE ];
    uint8_t cli_peer_public_key[ KEY_SIZE ];
    size_t olen;

    mbedtls_ecp_group G;
    mbedtls_ecp_point Qa;
    mbedtls_ecp_point Qb;
    mbedtls_mpi da;
    mbedtls_mpi za;

    mbedtls_base64_decode( cli_public_key, KEY_SIZE, &olen, ( uint8_t * )keys->client_public_key, strlen( keys->client_public_key ) );
    mbedtls_base64_decode( cli_private_key, KEY_SIZE, &olen, ( uint8_t * )keys->client_private_key, strlen( keys->client_private_key ) );
    mbedtls_base64_decode( cli_peer_public_key, KEY_SIZE, &olen, ( uint8_t * )keys->peer_public_key, strlen( keys->peer_public_key ) );

    prvReverseArray( cli_public_key, KEY_SIZE );
    prvReverseArray( cli_private_key, KEY_SIZE );
    prvReverseArray( cli_peer_public_key, KEY_SIZE );

    mbedtls_ecp_group_init( &G );
    mbedtls_ecp_point_init( &Qa );
    mbedtls_ecp_point_init( &Qb );
    mbedtls_mpi_init( &da );
    mbedtls_mpi_init( &za );

    if( ( ret = mbedtls_ecp_group_load( &G, MBEDTLS_ECP_DP_CURVE25519 ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_ecp_group_load(G): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_lset( &Qa.Z, 1 ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_lset(Qa): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_lset( &Qb.Z, 1 ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_lset(Qb): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_read_binary( &Qa.X, cli_public_key, KEY_SIZE ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_read_binary(Qa): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_read_binary( &Qb.X, cli_peer_public_key, KEY_SIZE ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_read_binary(Qb): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_read_binary( &da, cli_private_key, KEY_SIZE ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_read_binary(da): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }

    mbedtls_mpi_set_bit( &da, 0, 0 );
    mbedtls_mpi_set_bit( &da, 1, 0 );
    mbedtls_mpi_set_bit( &da, 255, 0 );
    mbedtls_mpi_set_bit( &da, 254, 1 );
    if( G.nbits == 254 )
    {
        mbedtls_mpi_set_bit( &da, 2, 0 );
    }

    if( ( ret = mbedtls_ecp_check_pubkey( &G, &Qa ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_ecp_check_pubkey(Qa): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_ecp_check_pubkey( &G, &Qb ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_ecp_check_pubkey(Qb): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_ecp_check_privkey( &G, &da ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_ecp_check_privkey(da): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_ecdh_compute_shared( &G, &za, &Qb, &da, NULL, NULL ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_ecdh_compute_shared(Qb, da): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }
    if( ( ret = mbedtls_mpi_write_binary( &za, shared_secret, KEY_SIZE ) ) != 0 )
    {
        configPRINTF( ( "mbedtls_mpi_write_binary(za): -0x%08X\n", ( unsigned int )-ret ) );
        return eCryptoFailure;
    }

    prvReverseArray( shared_secret, KEY_SIZE );
    return eCryptoSuccess;
}

AIACryptoErrorCode_t AIA_CRYPTO_Init( AIACrypto_t *crypto, AIACryptoKeys_t *keys )
{
    int ret;

    mbedtls_entropy_init( &crypto->entropy );
    mbedtls_ctr_drbg_init( &crypto->drbg );

    if( prvGenerateSharedSecret( keys ) == eCryptoFailure )
    {
        configPRINTF( ( "Failed to generate shared secret for encryption!\r\n" ) );
        goto init_fail;
    }

    /* Seed the PRNG using the entropy pool, and throw in our secret key as an
     * additional source of randomness. */
    ret = mbedtls_ctr_drbg_seed( &crypto->drbg, mbedtls_entropy_func, &crypto->entropy,
                                shared_secret, sizeof( shared_secret ) );
    if (ret != 0)
    {
        configPRINTF( ( "mbedtls_ctr_drbg_seed() returned -0x%04X\r\n", -ret ) );
        goto init_fail;
    }
    else
    {
        return eCryptoSuccess;
    }

init_fail:
    mbedtls_entropy_free( &crypto->entropy );
    mbedtls_ctr_drbg_free( &crypto->drbg );
    return eCryptoFailure;
}

void AIA_CRYPTO_Destory( AIACrypto_t *crypto )
{
    mbedtls_entropy_free( &crypto->entropy );
    mbedtls_ctr_drbg_free( &crypto->drbg );
}

int32_t AIA_CRYPTO_Encrypt( AIACrypto_t * crypto, void * msg_buf, void * plaintext, uint32_t plaintext_len, uint32_t sequence )
{
    int32_t ret;
    size_t cryptotext_len;
    AIAMessage_t * aia_msg = ( AIAMessage_t * )msg_buf;
    mbedtls_cipher_context_t ctx_enc;

    mbedtls_cipher_init( &ctx_enc );

    ret = mbedtls_cipher_setup( &ctx_enc, mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_AES_256_GCM ) );
    if( ret != 0 )
    {
        configPRINTF( ( "ctx_enc ,mbedtls_cipher_setup() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
        goto encrypt_exit;
    }

    /* The memory pointed to by plaintext contains preallocated memory preceding it for sequence number. */
    uint8_t * blob = ( uint8_t * )plaintext - AIA_MSG_PARAMS_SIZE_SEQ;
    memcpy( blob, ( void * )&sequence, AIA_MSG_PARAMS_SIZE_SEQ );

    ret = mbedtls_cipher_setkey( &ctx_enc, shared_secret, 256, MBEDTLS_ENCRYPT );
    if ( ret != 0 )
    {
        configPRINTF( ( "mbedtls_cipher_setkey() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
        goto encrypt_exit;
    }

    mbedtls_ctr_drbg_random( &crypto->drbg, aia_msg->iv, AIA_MSG_PARAMS_SIZE_IV );

    memcpy( aia_msg->sequence, ( void * )&sequence, AIA_MSG_PARAMS_SIZE_SEQ );

    ret = mbedtls_cipher_auth_encrypt(&ctx_enc,
                                      aia_msg->iv, AIA_MSG_PARAMS_SIZE_IV,
                                      NULL, 0,
                                      blob, plaintext_len + AIA_MSG_PARAMS_SIZE_SEQ,
                                      aia_msg->ciphertext, &cryptotext_len,
                                      aia_msg->mac, AIA_MSG_PARAMS_SIZE_MAC );
    if ( ret != 0 )
    {
        configPRINTF( ( "mbedtls_cipher_auth_encrypt() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
    }
    else
    {
        ret = cryptotext_len + ( ( void * )aia_msg->ciphertext - ( void * )aia_msg );
    }

encrypt_exit:
    mbedtls_cipher_free( &ctx_enc );
    return ret;
}

int32_t AIA_CRYPTO_Decrypt( AIACrypto_t * crypto,  void * msg_buf, const void * encrypted_msg, uint32_t encrypted_msg_len )
{
    int32_t ret;
    size_t decrypted_msg_len = 0;
    AIAMessage_t * aia_msg = ( AIAMessage_t * )encrypted_msg;
    mbedtls_cipher_context_t ctx_dec;

    mbedtls_cipher_init( &ctx_dec );

    ret = mbedtls_cipher_setup( &ctx_dec, mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_AES_256_GCM ) );
    if( ret != 0 )
    {
        configPRINTF( ( "ctx_dec ,mbedtls_cipher_setup() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
        goto decrypt_exit;
    }

    /* Get the length of encrypted content. */
    encrypted_msg_len -= ( void * )aia_msg->ciphertext - ( void * )aia_msg;

    ret = mbedtls_cipher_setkey( &ctx_dec, shared_secret, 256, MBEDTLS_DECRYPT );
    if( ret != 0 )
    {
        configPRINTF( ( "mbedtls_cipher_setkey() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
        goto decrypt_exit;
    }

    ret = mbedtls_cipher_auth_decrypt( &ctx_dec,
                                       aia_msg->iv, AIA_MSG_PARAMS_SIZE_IV,
                                       NULL, 0,
                                       aia_msg->ciphertext, encrypted_msg_len,
                                       ( uint8_t * )msg_buf, &decrypted_msg_len,
                                       aia_msg->mac, AIA_MSG_PARAMS_SIZE_MAC );

    if( ret != 0 )
    {
        configPRINTF( ( "mbedtls_cipher_authdecrypt() returned -0x%04X\r\n", -ret ) );
        ret = eCryptoFailure;
        goto decrypt_exit;
    }

    /* Check if the decrypted sequence number matches the unencrypted one. */
    if( memcmp( aia_msg->sequence, msg_buf, AIA_MSG_PARAMS_SIZE_SEQ ) != 0 )
    {
        configPRINTF( ( "Decrypted sequence number doesn't match the unencrypted one!\r\n" ) );
        ret = eCryptoSequenceNotMatch;
    }
    else
    {
        ret = decrypted_msg_len;
    }

decrypt_exit:
    mbedtls_cipher_free( &ctx_dec );
    return ret;
}
