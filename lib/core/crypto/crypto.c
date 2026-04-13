#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "crypto.h"
#include "vault.h"
#include "pico/rand.h"


#define CRYPTO_KNOWN_STR_LEN 23 // MUST BE <= 32
#define CRYPTO_ITER_NUM 100000 

const uint8_t known_string[] = "This statement is false"; //23 char, must be <=32






/*************************************************
 *  Setup Crypto Functions
*************************************************/

// vault must call before crypto_validate_passwd()
void crypto_prep_salt(uint8_t *salt) {
    secure_memset(salt + CRYPT_SALT_LEN, 0x00, 4);
    salt[19] = 0x01;
}


void crypto_get_16B_rand(uint8_t *array) {

    rng_128_t val;
    get_rand_128(&val);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 8; j++) {
            
            array[i * 8 + j] = (val.r[i] >> (j * 8)) & 0xFF;
        }
    }
}

void crypto_aes_init(mbedtls_aes_context *ctx, sha256_result_t *key) {
    mbedtls_aes_init(ctx);
    mbedtls_aes_setkey_enc(ctx, key->bytes, 256);
}

void crypto_aes_finish(mbedtls_aes_context *ctx) {
    mbedtls_aes_free(ctx);
}


/*************************************************
 *  AES-CTR Calculations
*************************************************/

bool crypto_encrypt_slice(mbedtls_aes_context *ctx, uint8_t *nonce, uint8_t *slice) {

    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    if (mbedtls_aes_crypt_ctr(ctx, CRYPT_SLICE_LEN, &nc_off, nonce, stream_block, slice, slice) != 0) {
        return false;
    }
    return true;
}




/*************************************************
 *  HMAC Calculations
*************************************************/


bool crypto_generate_hmac(uint8_t *salt, uint8_t *stored_hmac, hmac_mem_t *hmac_mem) {

    crypto_derive_key(salt, hmac_mem);

    memcpy(hmac_mem->password, hmac_mem->key.bytes, CRYPT_KEY_LEN);

    memcpy(hmac_mem->sha_buf + CRYPT_PASSWD_LEN, known_string, CRYPTO_KNOWN_STR_LEN);
    crypto_hmac_iter(hmac_mem->password, hmac_mem->sha_buf, CRYPTO_KNOWN_STR_LEN, &hmac_mem->key_buf);

    memcpy(stored_hmac, hmac_mem->key_buf.bytes, CRYPT_KEY_LEN);

    secure_memset(hmac_mem->password, 0x00, CRYPT_PASSWD_LEN);
    secure_memset(hmac_mem->key.bytes, 0x00, CRYPT_KEY_LEN);
    secure_memset(hmac_mem->key_buf.bytes, 0x00, CRYPT_KEY_LEN);

    return true;
}

// places final key in hmac_mem->key
bool crypto_validate_passwd(uint8_t *salt, uint8_t *stored_hmac, hmac_mem_t *hmac_mem) {

    crypto_derive_key(salt, hmac_mem);

    memcpy(hmac_mem->password, hmac_mem->key.bytes, CRYPT_KEY_LEN);

    memcpy(hmac_mem->sha_buf + 64, known_string, CRYPTO_KNOWN_STR_LEN);
    crypto_hmac_iter(hmac_mem->password, hmac_mem->sha_buf, CRYPTO_KNOWN_STR_LEN, &hmac_mem->key_buf);

    // now compare hmac_comp with stored_hmac
    if (!crypto_const_time_cmp(hmac_mem->key_buf.bytes, stored_hmac, CRYPT_KEY_LEN)) { //no match
        secure_memset(hmac_mem->password, 0x00, CRYPT_PASSWD_LEN);
        secure_memset(hmac_mem->key_buf.bytes, 0x00, CRYPT_KEY_LEN);
        secure_memset(hmac_mem->key.bytes, 0x00, CRYPT_KEY_LEN);
        return false;
    }

    secure_memset(hmac_mem->password, 0x00, CRYPT_PASSWD_LEN);
    secure_memset(hmac_mem->key_buf.bytes, 0x00, CRYPT_KEY_LEN);

    return true;
}


bool crypto_derive_key(uint8_t *salt, hmac_mem_t *hmac_mem) {

    memcpy(hmac_mem->sha_buf + CRYPT_PASSWD_LEN, salt, CRYPT_SALT_LEN + 4);
    crypto_hmac_iter(hmac_mem->password, hmac_mem->sha_buf, CRYPT_SALT_LEN + 4, &hmac_mem->key_buf);
    memcpy(hmac_mem->key.bytes, hmac_mem->key_buf.bytes, CRYPT_KEY_LEN);

    for (uint32_t i = 0; i < CRYPTO_ITER_NUM; i++) {

        memcpy(hmac_mem->sha_buf + CRYPT_PASSWD_LEN, hmac_mem->key_buf.bytes, CRYPT_KEY_LEN);
        crypto_hmac_iter(hmac_mem->password, hmac_mem->sha_buf, CRYPT_KEY_LEN, &hmac_mem->key_buf);
        crypto_xor_arrays(hmac_mem->key_buf.bytes, hmac_mem->key.bytes, CRYPT_KEY_LEN);
    }
    // hmac_mem->key should now be the final key

    secure_memset(hmac_mem->password, 0x00, CRYPT_PASSWD_LEN);
    secure_memset(hmac_mem->key_buf.bytes, 0x00, CRYPT_KEY_LEN);

    return true;
}


// key_in = [64B], in_out = [96B]
//  MUST store message in in_out+64 before calling this.
static bool crypto_hmac_iter(uint8_t *key_in, uint8_t *in_out, uint8_t msg_len, sha256_result_t *key_out) {

    if (msg_len > CRYPT_KEY_LEN) return false;

    memcpy(in_out, key_in, CRYPT_PASSWD_LEN);
    crypto_xor_64(in_out, 0x36);
    crypto_sha256(in_out, CRYPT_PASSWD_LEN + msg_len, key_out);

    secure_memset(in_out, 0x00, CRYPT_PASSWD_LEN + CRYPT_KEY_LEN);

    memcpy(in_out + CRYPT_PASSWD_LEN, key_out->bytes, CRYPT_KEY_LEN);
    memcpy(in_out, key_in, CRYPT_PASSWD_LEN);
    crypto_xor_64(in_out, 0x5C);
    crypto_sha256(in_out, CRYPT_PASSWD_LEN + CRYPT_KEY_LEN, key_out);
    secure_memset(in_out, 0x00, CRYPT_PASSWD_LEN + CRYPT_KEY_LEN);

    return true;
}

static bool crypto_sha256(uint8_t *data, uint8_t data_len, sha256_result_t *key) {

    pico_sha256_state_t state;
    pico_sha256_start_blocking(&state, SHA256_BIG_ENDIAN, false); 
    pico_sha256_update_blocking(&state, data, data_len);
    pico_sha256_finish(&state, key);

    return true;
}



/*************************************************
 *  Array Manipulation
*************************************************/

// input WILL be modified
static void crypto_xor_64(uint8_t *input, uint8_t var) {
    for (uint8_t i = 0; i < 64; i++) {
        input[i] = input[i] ^ var;
    }
}

static void crypto_xor_arrays(uint8_t *a, uint8_t *b, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        b[i] = a[i] ^ b[i];
    }
}

// Returns true if matched, false otherwise.
static bool crypto_const_time_cmp(const uint8_t *a, const uint8_t *b, uint8_t len) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < len; i++) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}



