#pragma once

#include "pico/sha256.h"
#include "mbedtls/aes.h"

#define CRYPT_PASSWD_LEN 64
#define CRYPT_KEY_LEN 32
#define CRYPT_SALT_LEN 16
#define CRYPT_SLICE_LEN 16
#define CRYPT_NONCE_LEN 16
#define CRYPT_HMAC_LEN CRYPT_KEY_LEN

typedef struct hmac_mem hmac_mem_t;


/*************************************************
 *  Setup Crypto Functions
*************************************************/

// vault must call
void crypto_prep_salt(uint8_t *salt);

void crypto_get_16B_rand(uint8_t *array);

void crypto_aes_init(mbedtls_aes_context *ctx, sha256_result_t *key);

void crypto_aes_finish(mbedtls_aes_context *ctx);


/*************************************************
 *  AES-CTR Calculations
*************************************************/

bool crypto_encrypt_slice(mbedtls_aes_context *ctx, uint8_t *nonce, uint8_t *slice);

/*************************************************
 *  HMAC Calculations
*************************************************/

bool crypto_generate_hmac(uint8_t *salt, uint8_t *stored_hmac, hmac_mem_t *hmac_mem);

bool crypto_validate_passwd( uint8_t *salt, uint8_t *stored_hmac, hmac_mem_t *hmac_mem);

bool crypto_derive_key(uint8_t *salt, hmac_mem_t *hmac_mem);

// key_in = [64B], in_out = [96B]
static bool crypto_hmac_iter(uint8_t *key_in, uint8_t *in_out, uint8_t msg_len, sha256_result_t *key_out);

static bool crypto_sha256(uint8_t *data, uint8_t data_len, sha256_result_t *key);





/*************************************************
 *  Array Manipulation
*************************************************/

// input WILL be modified
static void crypto_xor_64(uint8_t *input, uint8_t var);

static void crypto_xor_arrays(uint8_t *a, uint8_t *b, uint8_t len);

static bool crypto_const_time_cmp(const uint8_t *a, const uint8_t *b, uint8_t len);

static inline void secure_memset(void *ptr, uint8_t val, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) *p++ = val;
}

