#include <stdio.h>
#include "pico/stdlib.h"

#include "mbedtls/aes.h"

#include "vault.h"
#include "crypto.h"
#include "prog.h"
#include "fat16.h"
#include "shell.h"
#include "usb_kbd.h"
#include "os_debug.h"
#include "vfs.h"

#define printf(...) ((void)0)


// SHN.VLT file values
#define VAULT_SHN_STAT_LEN 1
#define VAULT_SHN_SALT_LEN CRYPT_SALT_LEN
#define VAULT_SHN_HMAC_LEN CRYPT_HMAC_LEN
#define VAULT_SHN_NONCE_BASE_LEN 8
#define VAULT_SHN_LEN (VAULT_SHN_STAT_LEN + VAULT_SHN_SALT_LEN + VAULT_SHN_HMAC_LEN + VAULT_SHN_NONCE_BASE_LEN)
#define VAULT_SHN_STAT_OFFSET 0
#define VAULT_SHN_SALT_OFFSET VAULT_SHN_STAT_LEN
#define VAULT_SHN_HMAC_OFFSET (VAULT_SHN_SALT_OFFSET + VAULT_SHN_SALT_LEN)
#define VAULT_SHN_NONCE_OFFSET (VAULT_SHN_HMAC_OFFSET + VAULT_SHN_HMAC_LEN)
#define VAULT_NONCE_CLUST_OFFSET 8
#define VAULT_NONCE_SLICE_OFFSET 12

// FAT defines
#define VAULT_FAT_ATTR_OFFSET 0x0B
#define VAULT_FAT_DIR_LEN 32
#define VAULT_SLICE_LEN CRYPT_SLICE_LEN
#define VAULT_FAT_CLUST_OFFSET 0x1A
#define VAULT_FAT_DEL_ENTRY 0xE5
#define VAULT_FAT_LFN_ENTRY 0x0F

/** TODOS
 * TODO: Comments in vault_setup() & vault_encrypt()
 * 
 */


typedef struct {

    hmac_mem_t hmac;

    uint8_t stored_hmac[CRYPT_HMAC_LEN];
    uint8_t salt[CRYPT_SALT_LEN + 4]; // actual salt is 16B, but var has 4 more bytes for hmac calc
    uint8_t nonce[CRYPT_NONCE_LEN];

    uint8_t slice[CRYPT_SLICE_LEN];
    mbedtls_aes_context ctx;

} Vault_Memory;
static_assert(sizeof(Vault_Memory) <= PROG_MEM_SIZE, "Vault_Memory exceeds the 100kB shared memory bank!");

_Static_assert(sizeof(vault_status_t) == 1, "vault_status_t is not 1 byte");


static Vault_Memory *mem;

static uint32_t slicesPerCluster;
static vault_status_t vstatus;


/*************************************************
 *  Shell Commands
*************************************************/

bool vault_setup(const out_t *out) {

    vault_mem_init();

    vault_update_status();
    if (vstatus != VAULT_STATUS_DECRYPTED) {
        out->write(out->ctx, "Vault already encrypted. Cannot setup. Decrypt first");
        return false;
    }
    // now we know either, the directory is decrypted, or has never been setup

    // === Check if VAULT/ and SHN.VLT exist
    if (!vfs_exists("/VAULT")) {
        vfs_mkdir("/VAULT");
        vfs_set_attr("/VAULT", FS_ATTR_DIR | FS_ATTR_SYS);
    } else {
        // checks if a file is named /VAULT (bad)
        if (!vfs_is_dir("/VAULT")) {
            out->write(out->ctx, "Vault exists but is a file? What the hell did you do??");
            return false;
        }
    }

    // if SHN.VLT exists, we need to strip its attributes so we can read/write it easily
    if (vfs_exists("/SHN.VLT")) {
        vfs_set_attr("/SHN.VLT", FS_ATTR_STD);
    }

    // === Create SHN.VLT if needed
    file_desc_t fd = vfs_open("/SHN.VLT", FS_MODE_CREATE | FS_MODE_WRITE);
    if (fd < 1) {
        out->write(out->ctx, "SHN.VLT could not be opened");
        return false;
    }
    vfs_seek(fd, 0, FS_SEEK_START);

    vstatus = VAULT_STATUS_DECRYPTED;
    vfs_write(fd, (uint8_t *)&vstatus, 1);

    // create and write salt
    crypto_get_16B_rand(mem->salt); // this might actually take a bit
    vfs_write(fd, mem->salt, VAULT_SHN_SALT_LEN);

    // === Take user password
    if (!vault_take_password(out, mem->hmac.password)) {
        vfs_close(fd);
        return false;
    }
    // mem->hmac.password shoud have the user's password in it now
    out->write(out->ctx, "\nPassword accepted, generating meta file... (~6s)\n");

    // === Use password to create stored HMAC
    crypto_prep_salt(mem->salt);
    crypto_generate_hmac(mem->salt, mem->stored_hmac, &mem->hmac);
    // now mem->stored_hmac holds the stored HMAC
    vfs_write(fd, mem->stored_hmac, VAULT_SHN_HMAC_LEN);

    // === Generate the Base Nonce
    crypto_get_16B_rand(mem->slice);
    memcpy(mem->nonce, mem->slice, VAULT_SHN_NONCE_BASE_LEN);

    vfs_write(fd, mem->nonce, VAULT_SHN_NONCE_BASE_LEN);

    if (vfs_get_file_size(fd) != VAULT_SHN_LEN) {
        out->write(out->ctx, "SHN.VLT is wrong/corrupted");
        vfs_close(fd);
        return false;
    }
    vfs_close(fd);

    vfs_set_attr("/SHN.VLT", FS_ATTR_HID | FS_ATTR_RO | FS_ATTR_STD | FS_ATTR_SYS);

    out->write(out->ctx, "Vault setup complete.");

    vault_mem_clear();
    return true;
}



bool vault_encrypt(const out_t *out) {

    // check if we are in /VAULT/
    if (strncmp(vfs_getpwd(), "/VAULT/", 7) == 0) {
        out->write(out->ctx, "Cannot encrypt VAULT while in it. Aborting");
        return false; // if so, we CANNOT encrypt
    }

    bool ret_val = false;
    file_desc_t fd = 0;
    vault_mem_init();

    // === Verify setup is correct
    if (vfs_exists("/SHN.VLT")) {
        vault_update_status();
        if (vstatus != VAULT_STATUS_DECRYPTED) {
            out->write(out->ctx, "Vault is already encrypted. Decrypt first");
            goto cleanup_encrypt;
        }
        vfs_set_attr("/SHN.VLT", FS_ATTR_STD);
    } else {
        out->write(out->ctx, "Vault has not been setup. SHN.VTL missing");
        goto cleanup_encrypt;
    }

    if (!vfs_exists("/VAULT")) {
        out->write(out->ctx, "Vault has not been setup. VAULT/ missing");
        goto cleanup_encrypt;
    }

    fd = vfs_open("/SHN.VLT", FS_MODE_WRITE);
    if (fd < 1) {
        out->write(out->ctx, "SHN.VLT could not be opened");
        goto cleanup_encrypt;
    }

    vfs_seek(fd, 0, FS_SEEK_START);
    vstatus = VAULT_STATUS_IN_PROGRESS;
    vfs_write(fd, (uint8_t *)&vstatus, 1);

    vstatus = VAULT_STATUS_DECRYPTED; // this is for goto cleanup:
        // if we fail early, we want this status written to SHN.VLT

    // === Verify user password & generate key
    // this function places the final key in mem->hmac.key
    if (vault_verify_passwd(fd, out)) {
        out->write(out->ctx, "Password validated. Encrypting...");
    } else {
        out->write(out->ctx, "Password FAILED validation. Aborting encryption");
        goto cleanup_encrypt;
    }

    // === Encrypt the vault
    // get the base nonce
    vfs_seek(fd, VAULT_SHN_NONCE_OFFSET, FS_SEEK_START);
    vfs_read(fd, mem->nonce, VAULT_SHN_NONCE_BASE_LEN);

    crypto_aes_init(&mem->ctx, &mem->hmac.key);

    // printf("Encryption started...\n");

    fat_file_handle_t handle;
    fat_find_entry("/VAULT", &handle);
    if(!vault_encrypt_dir(handle.start_cluster)) {
        vstatus = VAULT_STATUS_ERROR;
        goto cleanup_encrypt;
    }

    out->write(out->ctx, " Complete");

    vstatus = VAULT_STATUS_ENCRYPTED;
    
    ret_val = true;

cleanup_encrypt:
    if (fd > 0) {
        vfs_seek(fd, VAULT_SHN_STAT_OFFSET, FS_SEEK_START);
        vfs_write(fd, (uint8_t *)&vstatus, 1);
        vfs_close(fd);
    }
    vfs_set_attr("/SHN.VLT", FS_ATTR_HID | FS_ATTR_RO | FS_ATTR_STD | FS_ATTR_SYS);

    // clear it all when you're done
    crypto_aes_finish(&mem->ctx); // safe to call this even if not init-ed
    secure_memset(mem->hmac.key.bytes, 0x00, CRYPT_KEY_LEN);
    vault_mem_clear();

    return ret_val;
}



bool vault_decrypt(const out_t *out) {

    bool ret_val = false;
    file_desc_t fd = 0;
    vault_mem_init();

    // === Verify setup is correct
    if (vfs_exists("/SHN.VLT")) {
        vault_update_status();
        if (vstatus != VAULT_STATUS_ENCRYPTED) {
            out->write(out->ctx, "Vault is already decrypted. Encrypt first");
            goto cleanup_decrypt;
        }
        vfs_set_attr("/SHN.VLT", FS_ATTR_STD);
    } else {
        out->write(out->ctx, "Vault has not been setup. SHN.VTL missing");
        goto cleanup_decrypt;
    }

    if (!vfs_exists("/VAULT")) {
        out->write(out->ctx, "Vault has not been setup. VAULT/ missing");
        goto cleanup_decrypt;
    }

    fd = vfs_open("/SHN.VLT", FS_MODE_WRITE);
    if (fd < 1) {
        out->write(out->ctx, "SHN.VLT could not be opened");
        goto cleanup_decrypt;
    }

    vfs_seek(fd, 0, FS_SEEK_START);
    vstatus = VAULT_STATUS_IN_PROGRESS;
    vfs_write(fd, (uint8_t *)&vstatus, 1);

    vstatus = VAULT_STATUS_ENCRYPTED; // this is for goto cleanup:
        // if we fail early, we want this status written to SHN.VLT

    // === Verify user password & generate key
    // this function places the final key in mem->hmac.key
    if (vault_verify_passwd(fd, out)) {
        out->write(out->ctx, "Password validated. Decrypting...");
    } else {
        out->write(out->ctx, "Password FAILED validation. Aborting decryption");
        goto cleanup_decrypt;
    }

    // === Decrypt the vault
    // get the base nonce
    vfs_seek(fd, VAULT_SHN_NONCE_OFFSET, FS_SEEK_START);
    vfs_read(fd, mem->nonce, VAULT_SHN_NONCE_BASE_LEN);

    crypto_aes_init(&mem->ctx, &mem->hmac.key);

    fat_file_handle_t handle;
    fat_find_entry("/VAULT", &handle);
    if(!vault_decrypt_dir(handle.start_cluster)) {
        vstatus = VAULT_STATUS_ERROR;
        goto cleanup_decrypt;
    }

    out->write(out->ctx, " Complete");

    vstatus = VAULT_STATUS_DECRYPTED; 

    // === Increment Base Nonce
    vault_inc_base_nonce(mem->nonce);
    vfs_seek(fd, VAULT_SHN_NONCE_OFFSET, FS_SEEK_START);
    vfs_write(fd, mem->nonce, VAULT_SHN_NONCE_BASE_LEN);
    
    ret_val = true;

cleanup_decrypt:
    if (fd > 0) {
        vfs_seek(fd, VAULT_SHN_STAT_OFFSET, FS_SEEK_START);
        vfs_write(fd, (uint8_t *)&vstatus, 1);
        vfs_close(fd); 
    }
    vfs_set_attr("/SHN.VLT", FS_ATTR_HID | FS_ATTR_RO | FS_ATTR_STD | FS_ATTR_SYS);

    // clear it all when you're done
    crypto_aes_finish(&mem->ctx); // safe to call this even if not init-ed
    secure_memset(mem->hmac.key.bytes, 0x00, CRYPT_KEY_LEN); 
    vault_mem_clear();

    return ret_val;
}








/*************************************************
 *  Vault Crypto Functions
*************************************************/

static bool vault_crypt_cluster(uint16_t cluster) {

    if (cluster < 0x0002 || cluster >= 0xFFF7) {
        debug_deposit("CRPT-CRIT-En/Decrypt failed due to invalid cluster: ", cluster, DBG_U32_DEC);
        return false;
    }

    uint32_t addr = fat_addr_given_cluster(cluster);
    printf("starting cluster %u cryption...\n", cluster);

    // add cluster to nonce (assumes base nonce already added to nonce)
    _insert_32b_be(mem->nonce, VAULT_NONCE_CLUST_OFFSET, (uint32_t)cluster);
    
    for (uint16_t i = 0; i < slicesPerCluster; i++) {

        fat_read_16B(addr + (i * VAULT_SLICE_LEN), mem->slice);

        // add "16B slice number" to nonce
        _insert_32b_be(mem->nonce, VAULT_NONCE_SLICE_OFFSET, (uint32_t)i);

        if (!crypto_encrypt_slice(&mem->ctx, mem->nonce, mem->slice)) {
            debug_deposit("CRPT-CRIT-En/Decrypting slice returned error from mbedtls_aes_crypt_ctr()", 0, DBG_NULL_VAR);
            // this is really really bad. Half en/decrypted data => forever lost data
            debug_deposit("CRPT-CRIT-Failure occurred at cluster: ", cluster, DBG_U32_DEC);
            debug_deposit("CRPT-CRIT-Failure occurred at slice: ", i, DBG_U32_DEC);
            // there are 2048 slices/cluster
            return false;
        }

        fat_write_16B(addr + (i * VAULT_SLICE_LEN), mem->slice);
    }

    printf("finished cluster cryption\n");

    return true;
}

// will end with the final key in mem->hmac.key
static bool vault_verify_passwd(file_desc_t fd, const out_t *out) {

    // === Take user password
    if (!vault_take_password(out, mem->hmac.password)) {
        return false;
    } // mem->hmac.password shoud have the user's password in it now
    out->write(out->ctx, "\nPassword accepted, validating password... (~6s)\n");

    // === load up salt and stored hmac variables
    // SHN.VLT is already open and passed to this function

    vfs_seek(fd, VAULT_SHN_SALT_OFFSET, FS_SEEK_START);
    vfs_read(fd, mem->salt, VAULT_SHN_SALT_LEN);

    vfs_seek(fd, VAULT_SHN_HMAC_OFFSET, FS_SEEK_START);
    vfs_read(fd, mem->stored_hmac, VAULT_SHN_HMAC_LEN);

    // === verify password
    crypto_prep_salt(mem->salt);
    bool return_val = crypto_validate_passwd(mem->salt, mem->stored_hmac, &mem->hmac);

    return return_val;
}

static bool vault_take_password(const out_t *out, uint8_t *dest) {

    secure_memset(dest, 0x00, 64);
    out->write(out->ctx, "Enter user password (<=64 char) (no backspace):\n");

    key_event_t key;
    key.keycode = 0x00;
    uint8_t key_count = 0;

    while (key_count <= CRYPT_PASSWD_LEN) {
        if (keyboard_queue_pop(&key)) {
            if (key.keycode == '\n') break;

            if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == 'c') {
                out->write(out->ctx, "User cancelled password entry");
                secure_memset(dest, 0x00, CRYPT_PASSWD_LEN);
                return false;
            }

            if (key_count == CRYPT_PASSWD_LEN) {
                out->write(out->ctx, "Password too long!");
                secure_memset(dest, 0x00, CRYPT_PASSWD_LEN);
                return false;
            }

            if (key.keycode >= '!' && key.keycode <= '~') {
                dest[key_count] = key.keycode;
                out->write(out->ctx, "*");
                key_count += 1;
            } else {
                out->write(out->ctx, "Invalid key!");
                secure_memset(dest, 0x00, CRYPT_PASSWD_LEN);
                return false;
            }
        }
    }

    return true;
}


static void vault_inc_base_nonce(uint8_t *nonce) {

    for (int8_t i = 7; i >= 0; i--) {

        nonce[i] += 1;
        if (nonce[i] != 0) break;
    }
}


/*************************************************
 *  Memory Functions
*************************************************/

static void vault_mem_init(void) {

    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);

    mem = (Vault_Memory *)prog_get_mem();

    uint32_t bytesPerCluster = fat_get_bytes_per_cluster();
    slicesPerCluster = bytesPerCluster / VAULT_SLICE_LEN;
}

static void vault_mem_clear(void) {
    secure_memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);
}


/*************************************************
 *  FAT16 Interface Commands
*************************************************/

static void print_array(uint8_t *arr) {
    for (uint8_t i = 0; i < 16; i++) {
        printf("%c", arr[i]);
    }
    printf("\n");
    
    for (uint8_t i = 0; i < 16; i++) {
        printf("%02X ", arr[i]);
    }
    printf("\n");

}

static bool vault_crypt_test(void) {
    uint16_t cluster = 0x6969;
    uint8_t test_string[] = "Can you read?!?!";

    printf("Before encryption:\n");
    print_array(test_string);

    _insert_32b_be(mem->nonce, VAULT_NONCE_CLUST_OFFSET, (uint32_t)cluster);

    // ENCRYPT
    _insert_32b_be(mem->nonce, VAULT_NONCE_SLICE_OFFSET, (uint32_t)69);

    crypto_encrypt_slice(&mem->ctx, mem->nonce, test_string);

    printf("After encryption:\n");
    print_array(test_string);

    // DECRYPT
    _insert_32b_be(mem->nonce, VAULT_NONCE_SLICE_OFFSET, (uint32_t)69);

    crypto_encrypt_slice(&mem->ctx, mem->nonce, test_string);

    printf("After decryption:\n");
    print_array(test_string);

    return true;
}

static bool vault_crypt_cluster_chain(uint16_t start_cluster) {

    uint16_t cluster = start_cluster;
    while (cluster != 0x0000 && cluster < 0xFFF7) {

        if (!vault_crypt_cluster(cluster)) return false;
        cluster = fat_next_cluster(cluster);
        printf("next cluster (in cluster_chain()): %u\n", cluster);
    }
    return true;
}



static bool vault_decrypt_dir(uint16_t start_cluster) {

    uint16_t cluster = start_cluster;
    bool end_of_dir = false;

    printf("start cluster: %u\n", cluster);

    if (!vault_crypt_cluster_chain(cluster)) return false;

    while (cluster != 0x0000 && cluster < 0xFFF7 && !end_of_dir) {

        uint16_t entries_per_cluster = fat_get_bytes_per_cluster() / VAULT_FAT_DIR_LEN;

        for (uint16_t i = 0; i < entries_per_cluster; i++) {

            fat_read_16B(fat_addr_given_cluster(cluster) + (i * VAULT_FAT_DIR_LEN), mem->slice);

            if (mem->slice[0] == 0x00) {
                end_of_dir = true;
                break;
            }
            if (mem->slice[0] == VAULT_FAT_DEL_ENTRY) continue;
            if (mem->slice[VAULT_FAT_ATTR_OFFSET] == VAULT_FAT_LFN_ENTRY) continue;

            if (mem->slice[VAULT_FAT_ATTR_OFFSET] & FS_ATTR_DIR) { // this is a subdirectory

                if (mem->slice[0] != '.') {
                    fat_read_16B(fat_addr_given_cluster(cluster) 
                        + (i * VAULT_FAT_DIR_LEN) + VAULT_SLICE_LEN, mem->slice);
                    if (!vault_decrypt_dir(_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)))) {
                        return false;
                    }
                }

            } else { // this is a file, wipe it all
                fat_read_16B(fat_addr_given_cluster(cluster) 
                    + (i * VAULT_FAT_DIR_LEN) + VAULT_SLICE_LEN, mem->slice);
                // in case it's an empty file that does not have a cluster yet (cluster = 0x0000)
                if (_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)) < 2) continue;
                if (!vault_crypt_cluster_chain(_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)))) {
                    return false;
                }
            }

        }
        cluster = fat_next_cluster(cluster);
        printf("next cluster: %u\n", cluster);
    }
    return true;
}



static bool vault_encrypt_dir(uint16_t start_cluster) {
    
    uint16_t cluster = start_cluster;
    bool end_of_dir = false;

    printf("start cluster: %u\n", cluster);

    while (cluster != 0x0000 && cluster < 0xFFF7 && !end_of_dir) {

        uint16_t entries_per_cluster = fat_get_bytes_per_cluster() / VAULT_FAT_DIR_LEN;

        for (uint16_t i = 0; i < entries_per_cluster; i++) {

            fat_read_16B(fat_addr_given_cluster(cluster) + (i * VAULT_FAT_DIR_LEN), mem->slice);

            if (mem->slice[0] == 0x00) {
                end_of_dir = true;
                break;
            }
            if (mem->slice[0] == VAULT_FAT_DEL_ENTRY) continue;
            if (mem->slice[VAULT_FAT_ATTR_OFFSET] == VAULT_FAT_LFN_ENTRY) continue;

            if (mem->slice[VAULT_FAT_ATTR_OFFSET] & FS_ATTR_DIR) { // this is a subdirectory

                if (mem->slice[0] != '.') {
                    fat_read_16B(fat_addr_given_cluster(cluster) 
                        + (i * VAULT_FAT_DIR_LEN) + VAULT_SLICE_LEN, mem->slice);
                    if(!vault_encrypt_dir(_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)))) {
                        return false;
                    }
                }

            } else { // this is a file, wipe it all
                fat_read_16B(fat_addr_given_cluster(cluster) 
                    + (i * VAULT_FAT_DIR_LEN) + VAULT_SLICE_LEN, mem->slice);
                // in case it's an empty file that does not have a cluster yet (cluster = 0x0000)
                if (_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)) < 2) continue;

                if (!vault_crypt_cluster_chain(_le16p(mem->slice + (VAULT_FAT_CLUST_OFFSET - VAULT_SLICE_LEN)))) {
                    return false;
                }
            }
        }

        if (!vault_crypt_cluster(cluster)) return false;
        cluster = fat_next_cluster(cluster);
        printf("next cluster: %u\n", cluster);
    }

    // because hitting the end of the directory does not mean we are in the last cluster of the dir
    while (cluster != 0x0000 && cluster < 0xFFF7) {
        if (!vault_crypt_cluster(cluster)) return false;
        cluster = fat_next_cluster(cluster);
        printf("next cluster: %u\n", cluster);
    }
    return true;
}

static void _insert_32b_be(uint8_t *array, size_t offset, uint32_t value) {
    array[offset]     = (value >> 24) & 0xFF;
    array[offset + 1] = (value >> 16) & 0xFF;
    array[offset + 2] = (value >> 8) & 0xFF;
    array[offset + 3] = value & 0xFF;
}

static uint16_t _le16p(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// checks SHN.VLT file for encryption status
void vault_update_status(void) {

    if (!vfs_exists("/SHN.VLT")) {
        vstatus = VAULT_STATUS_DECRYPTED;
        return;
    }

    file_desc_t fd = vfs_open("/SHN.VLT", FS_MODE_READ_ONLY | FS_MODE_FORCE);
    if (fd < 1) {
        vstatus = VAULT_STATUS_ERROR;
    }
    vfs_seek(fd, 0, FS_SEEK_START);

    uint8_t status_byte;
    vfs_read(fd, &status_byte, 1);
    
    vfs_close(fd);
    if (status_byte != VAULT_STATUS_DECRYPTED
        && status_byte != VAULT_STATUS_ENCRYPTED
        && status_byte != VAULT_STATUS_IN_PROGRESS) {
    
        status_byte = VAULT_STATUS_ERROR;
    }
    printf("status byte: %02X\n", status_byte);

    vstatus = (vault_status_t) status_byte;
}

vault_status_t vault_status(void) {
    
    return vstatus;
}

