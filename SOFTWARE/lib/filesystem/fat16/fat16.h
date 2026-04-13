#pragma once

#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "../fs_types.h"

// FILE HANDLE FLAGS
#define FAT_HANDLE_FLAG_VALID FS_VALID_FLAG
#define FAT_HANDLE_FLAG_VOL_FULL FS_VOL_FULL_FLAG // Volume or Directory full
#define FAT_HANDLE_FLAG_INVALID 0x00
#define FAT_HANDLE_FLAG_READ_ONLY FS_READ_ONLY_FLAG
#define FAT_HANDLE_FLAG_WRITE FS_WRITE_FLAG
#define FAT_HANDLE_FLAG_APPEND FS_APPEND_FLAG
#define FAT_HANDLE_FLAG_CREATE FS_CREATE_FLAG
#define FAT_HANDLE_FLAG_FORCE FS_FORCE_FLAG


typedef struct {
    char file_name[11];
    uint8_t attributes; // from dir entry
    uint32_t file_size;
    uint32_t dir_entry; // [byte address] LBA + offset
    uint16_t start_cluster;
    uint8_t flags; // for communication between uses
} fat_file_handle_t;

typedef enum {
    FAT_TIMESTAMP_CREATION,
    FAT_TIMESTAMP_LAST_WRITE,
} file_timestamp_t;

/*************************************************
 *  EXPOSED CACHE VALIDITY FLAGS
*************************************************/

extern bool sector_cache_valid;
extern bool dir_cache_valid;
extern bool fat_cache_valid;


/*************************************************
 *  EXPOSED FUNCTIONS
*************************************************/

/**
 * @brief Mounts SD Card. Calculates and stores statics from MBR & VBR
 * 
 * Returns false on failure. Dump the debug log to see why.
 * First boot will setup SPI for SD.
 * @param sd_soft_init true if we want to SD Card init. 
 * unecessary after a SD format, for example
 */
bool fat_mount_sd(bool sd_soft_init);

/**
 * @brief Assumes SD card was plugged in (runs sd_block_soft_init()) and mount failed.
 * 
 * This runs fat_mount_sd(false) & fat_init_os_sd() after formatting.
 */
bool fat_format_sd(void);

//flushes caches and deinits SD
bool fat_unmount_sd(void);



/**
 * @brief Fills item_handle with entry at provided path. Will NOT debug log if item not found.
 * 
 * This loop and find thing is all over this code. This is a late effort to put it in one
 * function. It isn't being used in a lot of places because the old code is working fine, but
 * this should get rolled in as updates or refactors happen
 * 
 * @param path ABSOLUTE PATHS ONLY. Must be NUL-TERMINATED.
 * @param item_handle duh
 * @return false - requested entry does not exist, or similar error
 */
bool fat_find_entry(const char * path, fat_file_handle_t *item_handle);





/**
 * @brief opens a file requested at the path, can create files if FAT_HANDLE_FLAG_CREATE
 * 
 * calls _find_dir_entry() which searches one directory for an entry. Remember file and 
 * folder names are all 8.3. Calls _get_next_entry_name to pull out char[11] name from path
 * for entry searching
 * 
 * @param path ABSOLUTE PATHS ONLY. Must be NUL-TERMINATED.
 * @param mode see fat_open_mode_t options
 * @return fat_file_handle_t handle containing: dir_entry (byte address), start cluster, item
 * name, item attributes, item size, and flags
 * 
 * Errors will return a flag != FAT_HANDLE_FLAG_VALID. Check for this. Dump debug log if you want
 * 
 * FAT_HANDLE_FLAG_VOL_FULL means it could not create the file requested, due to dir or vol full
 */
fat_file_handle_t fat_open(const char *path, file_open_mode_t mode);

/**
 * @brief Reads bytes from a file (handle) into buffer (dst)
 * 
 * ENSURE, offset + len <= file_size. Can fail if gen_cache_valid is false. 
 * 
 * @param handle duh
 * @param offset [bytes]
 * @param dst duh
 * @param len [bytes]
 * @return int32_t number of bytes read. -1 = reading hit out of range (volume or cluster chain)
 */
int32_t fat_read(fat_file_handle_t *handle, uint32_t offset, uint8_t *dst, uint32_t len);

/**
 * @brief Writes bytes from buffer (src) into file (handle). Will claim clusters as necessary
 * 
 * Has very unlikely "edge of cluster" bug, see code. Writes new file_size to handle
 * 
 * @param handle duh
 * @param offset [bytes]
 * @param src duh
 * @param len [bytes]
 * @return int32_t number of bytes written. -1 = write failed, path typo, or offset outside of file
 */
int32_t fat_write(fat_file_handle_t *handle, uint32_t offset, const uint8_t *src, uint32_t len);

/**
 * @brief If a file shrinks, this function can deallocate its clusters. Updates handle.file_size as well.
 * 
 * DO NOT change handle.file_size before running this. This funciton will update
 * handle.file_size for you. Runs fat_close() as well.
 * 
 * @param handle where it pulls old/original file size from
 * @param new_size [bytes]
 * @return based solely on fat_close()
 */
bool fat_truncxtend(fat_file_handle_t *handle, uint32_t new_size);

// Writes all handle data (name, size, attribute, cluster, timestamps) to directory entry. Calls fat_flush()
bool fat_close(fat_file_handle_t *handle); 

/**
 * @brief Calls _create_dir() in the right directory
 * 
 * @param path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @return false - if path wrong, vol full, or file name too long
 */
bool fat_mkdir(const char *path);

/**
 * @brief CANNOT remove if non-empty. Finds directory and deletes entry in parent and deallocates clusters
 * 
 * Calls fat_close() to write directory entry as 0xE5 in parent directory
 * 
 * @param path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @return false - if bad path, tried to remove root dir, or if dir not empty
 */
bool fat_rmdir(const char *path);

/**
 * @brief opens a file, deallocates all clusters, marks as deleted (0xE5)
 * 
 * @param path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @return false - if handle from fat_open() is invalid, or if fat_truncxtend() fails
 */
bool fat_delete(const char *path);

/**
 * @brief returns timestamp of file
 * 
 * @param handle duh
 * @param option TIMESTAMP_CREATION or TIMESTAMP_LAST_WRITE
 * @param ts_bytes returns 4 bytes of timestamp. Use timestamp_struct_tm() to convert
 * @return false pull failed or timestamp does not exist (all 0x00's)
 */
bool fat_timestamp_bytes(fat_file_handle_t *handle, file_timestamp_t option, uint8_t *ts_bytes);

// converts 4 byte time & date from directory entry to struct tm. Accounts for little endian
void fat_timestamp_tm(const uint8_t *bytes, struct tm *now);

/**
 * @brief Opens dir and saves off starting sector as library static. Call fat_listdir() very soon
 * 
 * saves off first sector of directory AND returns number of directory entries for # fat_listdir() calls
 * into "dir" object. If you only want the number of entries (real/fake), pass NULL for "dir"
 * 
 * @param path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @param real_files_only excludes volume name, current dir (.), and parent dir (..), AND deleted
 * items from return value
 * @param dir directory object to pass when you call fat_listdir() repeatedly
 * @return int32_t number of directory entries INCLUDING volume name, current dir (.), and parent dir (..).
 *  -1 if path wrong, bad cluster in dir
 */
int32_t fat_opendir(const char *path, bool real_files_only, fat_dir_list_t *dir);

/**
 * @brief Called multiple times to return each item in directory. Call fat_opendir() first
 * 
 * Calls _load_file_handle(). Caller must filter out volume name, current dir (.), and parent dir (..)
 * if the caller wants to.
 * 
 * @param dir directory object filled in by fat_opendir(). USER MUST INCREMENT dir.index
 * @param entry handle that gets filled with item data 
 * @return false means index called is out of bounds (called too many times)
 */
bool fat_listdir(fat_dir_list_t *dir, fat_file_handle_t *entry);

// syncs all caches with SD card
bool fat_flush(void);


/**
 * @brief opens src file, creates new dst file, moves directory entry to new location
 * 
 * Can be used to rename files. Can not move folders
 * 
 * @param src_path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @param dst_path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @return false means volume is full or you made a typo, or can't open src file
 */
bool fat_move(const char *src_path, const char *dst_path); 

/**
 * @brief opens src & dst (creates if doesn't exist) files, copies data, closes files
 * 
 * Uses spare_buf[FAT_SPARE_BUF_SIZE] (256) to fat_read() then fat_write() data between files
 * 
 * @param src_path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @param dst_path ABSOLUTE PATH. Must be NUL-TERMINATED
 * @return false if any of the fat_read() or fat_write() fail, or if can't open dst/src file
 */
bool fat_copy(const char *src_path, const char *dst_path);

// returns 0-100 [%] of clusters claimed, helpful to see free space, not fully accurate
uint8_t fat_cluster_usage(void); 

// to set up necessary files/folders for OS
bool fat_init_os_sd(void); 

// ENTRY ATTRIBUTE MODS:

bool fat_get_attr(const char *path, uint8_t *attr);

bool fat_set_attr(const char *path, uint8_t attr);


/*************************************************
 *  READ/WRITE 16B from SD (used in vault & encryption)
*************************************************/

bool fat_read_16B(uint32_t addr, uint8_t *buf);

bool fat_write_16B(uint32_t addr, uint8_t *buf);

uint32_t fat_addr_given_cluster(uint16_t cluster);

uint16_t fat_next_cluster(uint16_t prev_cluster);

uint32_t fat_get_bytes_per_cluster(void);



/*************************************************
 *  HELPER FUNCTIONS
*************************************************/

// converts little endian 32-bit value to big endian (pointer)
static uint32_t _le32p(const uint8_t *p);
static uint32_t _le32(uint32_t val);
// converts little endian 16-bit value to big endian (pointer)
static uint16_t _le16p(const uint8_t *p);
static uint16_t _le16(uint16_t val);
// writing little endian values to buffer
static void w_le16(uint8_t *buf, size_t offset, uint16_t val);
static void w_le32(uint8_t *buf, size_t offset, uint32_t val);

// FAT Calculations
static uint32_t _locate_n_FAT(uint8_t fat_copy);
static uint16_t _num_FAT_entries(void);
static uint32_t _FAT_sect_of_cluster(uint8_t fat_copy, uint16_t cluster);
static uint32_t _sector_given_cluster(uint16_t cluster);
static uint32_t _cluster_given_sector(uint32_t sector);

// FINDING THINGS:

/**
 * @brief Finds requested (entry_name) in directory at sector (dir_sector) and stores in *handle
 * 
 * Does not filter out LFN entries. If item not found, handle stores dir_entry_addr of entry
 * where new file will go. Calls _load_file_handle() to fill handle. Calls _next_sect_of_item()
 * to keep searching through sectors of directory for entry. Will claim new cluster if searching 
 * in directory hits end of cluster but not 512 items
 * 
 * @param handle handle pointer to store item details, using _load_file_handle()
 * @param dir_sector first sector of directory we are searching in
 * @param entry_name name to look for (8.3)
 * @return uint32_t useless. Doesn't mean anything. Is not used.
 */
static int32_t _find_dir_entry(fat_file_handle_t *handle, uint32_t dir_sector,const char *entry_name);

/**
 * @brief Takes dir_entry_addr (byte address) and loads it into *handle
 * 
 * @param dir_entry_addr byte address of directory entry
 * @param handle handle pointer
 * @return always true, no false
 */
static bool _load_file_handle(uint32_t dir_entry_addr, fat_file_handle_t *handle);

/**
 * @brief Finds next sector of file or directory. CAN jump clusters, but usually just goes to 
 * next sequential sector
 * 
 * @param dir_sector 
 * @return uint32_t 0 = bad cluster or found sector is out of range, error. 1 = entered sector
 * was last sector of last cluster (cluster was 0xFFFF). Otherwise it returns the next sector
 */
static uint32_t _next_sect_of_item(uint32_t dir_sector);

/**
 * @brief Strips out entry name from path into file_name format (11 char, no '.')
 * 
 * Breaks at / or \0
 * 
 * @param path the path to pull out the name
 * @param path_index where to start search in path
 * @param entry_name pointer to fill with file_name
 * @return uint16_t new path_index for next search/loop
 */
static uint16_t _get_next_entry_name(const char *path, uint32_t path_index, uint8_t *entry_name);



// CREATING THINGS:

/**
 * @brief Creates file with dir entry. Claimes NO cluster though (fat_write() does that)
 * 
 * Also does job of _load_file_handle() but for new files. Standard 0x20 dir entry attribute.
 * No "file creation time" yet...
 * 
 * @param file_name duh
 * @param handle comes in with dir_entry (byte address)
 * @return false if directory full (512 entries or no avail clusters for directory's new entry)
 */
static bool _create_file(const char *file_name, fat_file_handle_t *handle);

/**
 * @brief Creates folder at handle.dir_entry byte addresss
 * 
 * @param dir_name 8 char
 * @param handle contains dir_entry with byte address of new dir entry
 * @return false - if dir name is too long or volume is full
 */
static bool _create_dir(const char *dir_name, fat_file_handle_t *handle);

/**
 * @brief Claims next available cluster. Also updates current cluster entry to point to new one
 * 
 * @param last_cluster the prev last cluster of current item
 * @return new cluster. 0 = volume is full, no avail cluster
 */
static uint16_t _claim_avail_cluster(uint16_t last_cluster);

// trims last '/' some users put at the end of paths that end with directory
static bool _trim_terminal_slash(char *path);

// converts LBA data into CHS for MBR partition entry
static void _lba_to_chs(uint32_t lba, uint8_t chs[3]);

// generates MBR partition entry
static void _mbr_partition_entry(uint32_t start_lba, uint32_t lba_count, uint8_t *entry16 );

// converts struct tm into 4 byte time & date for directory entry. Accounts for little endian
static void _timestamp_dir_entry(const struct tm *now, uint8_t *bytes);

// wipes all statics after unmounting so fat/sd calls don't try to write/read from dangerous areas
static void _fat_wipe_statics(void);





// QUICK CHECKS:

static bool _is_entry_directory(uint8_t attr);
static bool _entry_is_not_deleted(uint8_t dir_first_byte, uint8_t dir_attr_byte);
static bool _fat_read_only_mode(file_open_mode_t mode);

// loading caches:
static uint8_t* _load_dir_cache(uint32_t lba);
static uint8_t* _load_fat_cache(uint32_t lba);
static uint8_t* _load_gen_cache(uint32_t lba);

// writing to caches:
static uint8_t* _write_dir_cache(uint32_t lba);
static uint8_t* _write_fat_cache(uint32_t lba);
static uint8_t* _write_gen_cache(uint32_t lba);
