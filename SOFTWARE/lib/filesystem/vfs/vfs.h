#pragma once

#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"

#include "../fs_types.h"

#define VFS_FILE_NAME_LEN 13
#define VFS_FILE_SIZE_LEN 9
#define VFS_FILE_TS_LEN 16
#define VFS_FILE_ATTR_LEN 3

typedef int8_t file_desc_t;

typedef enum {
    FS_SEEK_START, // set offset from start of file
    FS_SEEK_CURRENT, // set offset from current position/offset
    FS_SEEK_END, // set offset from end of file
} fs_offset_origin_t;

typedef enum {
    FS_ERROR_INVALID,
    FS_ERROR_VOL_FULL,
} fs_error_flag_t;

typedef enum {
    FS_FILE_NAME,
    // FS_FILE_NAME_SIZE,
    // FS_FILE_NAME_MTIME,
    // FS_FILE_NAME_SIZE_MTIME,
    FS_FILE_NAME_SIZE_MTIME_ATTR,
} fs_file_fields_t;

typedef struct {
    char name[VFS_FILE_NAME_LEN + 1]; //+1 for nul terminator
    char size[VFS_FILE_SIZE_LEN + 1];
    char ts[VFS_FILE_TS_LEN + 1];
    char attr[VFS_FILE_ATTR_LEN + 1];
} vfs_dir_entry_t;

typedef struct {
    uint8_t create_time[4]; 
    uint8_t modify_time[4]; 
    uint32_t file_size; 
    uint8_t attribute;
} fs_out_stat_t;

/**
 * RULES:
 * All path lengths must be <= 256 chars 
 */



// TODO: listing files
//          this will take coordination between a shell command, vfs, and fat16

// calls fat_mount_sd(true) and sets pwd to "/"
bool vfs_init(void);

/**
 * @brief Takes relative or absolute path to be new current working directory
 * 
 * @param path case IN-sensitive, max length = VFS_MAX_PATH_LEN (256) chars
 * @return false - path does not exist OR typo
 */
bool vfs_chdir(const char *path);

/**
 * @brief Returns current working directory as a string
 * 
 * @return const char* NUL-terminated
 */
const char* vfs_getpwd(void);

/////////////////

/**
 * @brief Opens a file at path. Will create if mode is set right. Adds fat file handle, 
 * offset, and in_use flag to entry in File Descriptor Table (max 7 open at once)
 * 
 * @param path 256 char max
 * @param mode FS_MODE_WRITE, _READ_ONLY, _CREATE, _APPEND, _FORCE
 * @return file_desc_t -1 = failed, 0 = file already opened (try read only)
 */
file_desc_t vfs_open(const char *path, file_open_mode_t mode);

// Removes entry from fd table
bool vfs_close(file_desc_t fd);

/////////////////

/**
 * @brief sets the offset for a file descriptor from a given point.
 * 
 * @param fd duh
 * @param offset in bytes (positive or negative)
 * @param origin FS_SEEK_START, _CURRENT, _END
 * @return false - you did bad math
 */
bool vfs_seek(file_desc_t fd, int32_t offset, fs_offset_origin_t origin);

// just returns the fd's offset
uint32_t vfs_tell(file_desc_t fd);

// reads "len" bytes into "dst" buffer from "fd" file descriptor
int32_t vfs_read(file_desc_t fd, uint8_t *dst, uint32_t len);

// reads "len" bytes into "dst" buffer from "fd" file descriptor
int32_t vfs_write(file_desc_t fd, uint8_t *src, uint32_t len);

// for shortening files. Calls fat_truncxtend(). Will move fd.offset
bool vfs_truncate(file_desc_t fd, uint32_t new_size);

/////////////////

// returns stat struct for file at given path
bool vfs_stat_path(const char *path, fs_out_stat_t *stats);

// returns stat struct for file at given file descriptor. Requires file to be open.
//      Special exception for file open at fd_table[0]
bool vfs_stat_fd(file_desc_t fd, fs_out_stat_t *stats);

// takes timestamp_bytes from stat struct and returns a struct tm timestamp
bool vfs_timestamp_tm(uint8_t *ts_bytes, struct tm *timestamp);

////////////////

uint32_t vfs_get_file_size(file_desc_t fd);

// returns pointer to file_name in file handle. NOT NUL terminated
char * vfs_get_file_name(file_desc_t fd);

////////////////

// deletes a file after validating it exsts and is not in_use in the fd table
bool vfs_delete(const char *path, bool force);

// creates a directory if it does not already exist
bool vfs_mkdir(const char *path);

// removes a directory if it has no files in it
bool vfs_rmdir(const char *path, bool force);

/**
 * @brief Moves a file from src_path to dst_path. Can be used to rename files
 * 
 * src_path file can be in use, but will be flushed to disk. dst_path file must
 * NOT be in use if it exists
 */
bool vfs_move(const char *src_path, const char *dst_path, bool force);

/**
 * @brief Copies a file from src_path to dst_path.
 * 
 * src_path file can be in use, but will be flushed to disk. dst_path file must
 * NOT be in use if it exists
 */
bool vfs_copy(const char *src_path, const char *dst_path);

// syncs all file data to disk using fat_close(). Does NOT actually close the file.
bool vfs_fsync(file_desc_t fd);

// returns true if the given file or directory exists
bool vfs_exists(const char *path);

// checks if the path is a directory. DOES NOT verify path exists
//      that means: both path to file and path DNE return FALSE. You've been warned
bool vfs_is_dir(const char *path);

// do not use
fs_error_flag_t vfs_errno(file_desc_t fd);

//////////////////////// LISTING

/**
 * @brief Fills in passed "dir" struct to call vfs_listdir() many times
 * 
 * Do not modify what is in dir struct.
 * 
 * @param path max of 256 chars
 * @param dir pointer to one
 * @return true 
 * @return false 
 */
bool vfs_opendir(const char *path, fat_dir_list_t *dir);

/**
 * @brief Will return vfs_dir_entry_t struct with data requested by fields.
 * Keep calling until false is returned. DO NOT increment index.
 * 
 * Caller must themselves filter out "." and ".." entries if they want to
 * 
 * @param dir filled in with vfs_opendir()
 * @param fields FS_FILE_NAME, _SIZE, _MOD_TIME, _CREATE_TIME
 * @param entry_out entries with no data will just be spaces ' '
 * @return false - listing is done, move on
 */
bool vfs_listdir(fat_dir_list_t *dir, fs_file_fields_t fields, vfs_dir_entry_t *entry_out, bool all_flag);


/**
 * @brief actually parses file "fd" details (file name, file size, modified time) into 
 * "entry" object based on the fields requested
 */
bool _vfs_parse_fd(file_desc_t fd, fs_file_fields_t fields, vfs_dir_entry_t *entry);

//////////////// ATTRIBUTE MODS

bool vfs_get_attr(const char *path, uint8_t *attr);

bool vfs_set_attr(const char *path, uint8_t attr);




// PATH HELPER FUNCTIONS:

/**
 * @brief calls _vfs_build_abs_path() & _vfs_clean_path()
 * 
 * @param path_in user entered path, usually
 * @param abs_path_out mostly properly formated path
 * @return false - typo, does NOT check if path exists
 */
static bool _vfs_resolve_path(const char *path_in, char *abs_path_out);

/**
 * @brief Takes user path_in and builds just a long path_out
 * 
 * The path may be full of "." ".." "//". _vfs_clean_path() will clean that
 */
static bool _vfs_build_abs_path(const char *path_in, char *path_out);

/**
 * @brief Cleans path of "." ".." and "//" if they are there
 * 
 * Edits in place as the final version is always shorter. 
 */
static bool _vfs_clean_path(char *path_out);


//Checks if a path starts with "/". If so it must be an absolute path
static inline bool _vfs_is_abs_path(const char *path);

// Takes a path and path length and capitalizes all a-z characters
static bool _vfs_capitalize_path(char *path, uint16_t path_len);

/**
 * @brief generates FAT6 compliant path from path_in
 * 
 * Will capitalize, adjudiacte ".."s, calculate path length, and add trailing "/"
 *      if the path points to a directory (see is_file)
 * 
 * @param path_out usually scratch_pwd or scratch_pwd2, will be wiped
 * @param is_file removes trailing "/" if necessary
 * @return int32_t path_out length (-1 = failed!)
 */
int32_t vfs_build_valid_path(const char *path_in, char *path_out, bool is_file);

/**
 * @brief returns true if the path is a valid 8.3 name. 
 * 
 * Only checks last item in path, as that is what the user wants created. Assumes directories
 * have terminating "/" and files do NOT. If not, this breaks (returns false)
 * 
 * 8.3 Rules: directories do NOT have '.' only name with <=8 chars. Files have <=8 char
 * file names and <=3 char extensions
 * 
 * @param is_file is known because of _vfs_build_valid_path() callers know this
 * @return true file/dir name is valid
 */
static bool _vfs_valid_83_name(const char *path, uint16_t path_len);