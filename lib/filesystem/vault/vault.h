#pragma once

#include "pico/sha256.h"
#include "vfs.h"

typedef struct hmac_mem {

    uint8_t password[64];
    uint8_t sha_buf[96];
    sha256_result_t key_buf;
    sha256_result_t key;

} hmac_mem_t;

typedef enum  vault_status {
    VAULT_STATUS_DECRYPTED = 0,
    VAULT_STATUS_ENCRYPTED = 1,
    VAULT_STATUS_IN_PROGRESS = 2,
    VAULT_STATUS_ERROR = 3,
} __attribute__ ((packed)) vault_status_t;

typedef struct shell_out out_t;


/*************************************************
 *  Shell Commands
*************************************************/

/**
 * @brief Lets user enter new password and creates new SHN.VLT file and VAULT/ dir.
 * Will fail if a VAULT/ already exists and is encrypted
 * 
 * @param out passed in by shell so func and output status to user
 * @return false - process failed
 */
bool vault_setup(const out_t *out);

/**
 * @brief Will encrypt the VAULT/ directory
 * 
 * @param out passed in by shell so func and output status to user
 * @return false - process failed
 */
bool vault_encrypt(const out_t *out);

/**
 * @brief Will decrypt the VAULT/ directory
 * 
 * @param out passed in by shell so func and output status to user
 * @return false - process failed
 */
bool vault_decrypt(const out_t *out);






/*************************************************
 *  Vault Crypto Functions
*************************************************/

// en/decrypts only the given cluster. AES-CTR is reflexive this function does double duty
static bool vault_crypt_cluster(uint16_t cluster);

// verifies the user password matches the SHN.VTL copy using a stored HMAC
static bool vault_verify_passwd(file_desc_t fd, const out_t *out);

// takes the user password (<= 64 char) by robbing the keyboard queue
static bool vault_take_password(const out_t *out, uint8_t *dest);

// increments base nonce. To be called after a decryption
static void vault_inc_base_nonce(uint8_t *nonce);

/*************************************************
 *  Memory Functions
*************************************************/

// Initializes prog mem for this library (even though it is technically not a program)
// also initializes some variables
static void vault_mem_init(void);

// Clears prog mem
static void vault_mem_clear(void);


/*************************************************
 *  FAT16 Interface Commands
*************************************************/

// bool vault_crypt_test(void); //REMOVE

// encrypts a FAT16 cluster chain
static bool vault_crypt_cluster_chain(uint16_t start_cluster);

// encrypts a FAT16 directory, given the directory's starting cluster
static bool vault_encrypt_dir(uint16_t start_cluster);

// decrypts a FAT16 directory, given the directory's starting cluster
static bool vault_decrypt_dir(uint16_t start_cluster);

// for converting from little endian
static uint16_t _le16p(const uint8_t *p);

// copies uint32_t into 4 entries in a uint8_t array
static void _insert_32b_be(uint8_t *array, size_t offset, uint32_t value);

// uses VFS to open and read vault status from the SHN.VLT file
void vault_update_status(void);

// returns just a variable that must be updated with vault_update_status() recently
vault_status_t vault_status(void);