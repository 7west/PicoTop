#include <string.h>

#include "vfs.h"
#include "fat16.h"
#include "os_debug.h"
#include "vault.h"

#define printf(...) ((void)0)

#define VFS_NUM_FILE_DESCRIPTIONS 8
#define VFS_FD_STAGE 0

typedef struct {
    fat_file_handle_t handle;
    uint32_t offset;
    bool in_use; // not needed?
} vfs_file_desc_t;


// File Descriptor Table
static vfs_file_desc_t fd_table[VFS_NUM_FILE_DESCRIPTIONS] = {0};

// +1 to all paths to guarentee nul termination and easier math elsewhere
static char pwd[VFS_MAX_PATH_LEN+1] = {0};
static uint16_t pwd_len = 0;
static char scratch_pwd[VFS_MAX_PATH_LEN+1] = {0};
static char scratch_pwd2[VFS_MAX_PATH_LEN+1] = {0}; // when we need two paths (move & copy)

// used only in _vfs_clean_path() but we don't want something
//  this big coming out of the stack
static uint16_t seg_starts[VFS_MAX_PATH_LEN / 2];

/*************************************************
 *  vfs_init()
*************************************************/

bool vfs_init(void) {
    pwd[0] = '/'; // set pwd to root dir
    pwd_len = 1;

    if (fat_mount_sd(true)) {
        debug_deposit("VFSL-INFO-Virtual File System initialized successfully", 0, DBG_NULL_VAR);
        return true;
    } else {
        debug_deposit("VFSL-CRIT-Virtual File System failed to initialize", 0, DBG_NULL_VAR);
        return false;
    }
}

/*************************************************
 *  vfs_chdir() and its helpers
*************************************************/

bool vfs_chdir(const char *path) {

    int32_t new_pwd_len = vfs_build_valid_path(path, scratch_pwd2, false);
    if (new_pwd_len < 0) return false;

    printf("moving to path: %s\n", scratch_pwd2);

    // check if VAULT is encrypted and if desired path starts with VAULT
    if ((vault_status() != VAULT_STATUS_DECRYPTED) && (strncmp(scratch_pwd2, "/VAULT/", 7) == 0)) {
        return false; // if so, do NOT go in there
    }
    // NOTE: that vault_status() uses vfs, and consequently scratch_pwd, to open SHN.VLT
    //      This is very bad, which is why we use scratch_pwd2

    // verify directory exists
    if (fat_opendir(scratch_pwd2, false, NULL) == -1) return false;

    // copy scratch_pwd into pwd (and the length)
    memcpy(pwd, scratch_pwd2, new_pwd_len + 1);
    pwd_len = (uint16_t) new_pwd_len;
    
    return true;
}

static bool _vfs_capitalize_path(char *path, uint16_t path_len) {

    for (uint16_t i = 0; i < path_len; i++) {
        if (path[i] >= 'a' && path[i] <= 'z') {
            path[i] -= 32;
        }
    }

    return true;
}

static inline bool _vfs_is_abs_path(const char *path) {
    return path && path[0] == '/';
}

static bool _vfs_build_abs_path(const char *path_in, char *path_out) {

    if (!path_in || !path_out) return false;
    
    size_t path_in_len = strnlen(path_in, VFS_MAX_PATH_LEN);
    if (path_in_len >= VFS_MAX_PATH_LEN) return false;

    // first check if user passed absolute path. if so, our job is easy
    if (_vfs_is_abs_path(path_in)) {
        
        memcpy (path_out, path_in, path_in_len + 1);
        return true;
    }

    if (pwd_len == 0 || pwd_len >= VFS_MAX_PATH_LEN) return false;

    //move pwd into path_out (scratch_pwd), it is already absolute
    memcpy(path_out, pwd, pwd_len);

    uint16_t write_pos = pwd_len;
    // ensure there is only one '/' between base pwd and new relative path
    if (write_pos > 1 && path_out[write_pos - 1] != '/') {
        if (write_pos + 1 >= VFS_MAX_PATH_LEN) return false;
        path_out[write_pos++] = '/';
    }

    const char *path_in_ptr = path_in;
    // jump over / or // or /// etc.
    while (*path_in_ptr == '/') path_in_ptr++;

    // find path_in_len left in path_in to append to path_out (scratch_pwd)
    path_in_len -= (size_t)(path_in_ptr - path_in);
    // note: path_in_len is now the path_in left to write and does NOT represent the path_in length anymore
    
    if (write_pos + path_in_len >= VFS_MAX_PATH_LEN) return false;
    memcpy(path_out + write_pos, path_in_ptr, path_in_len + 1);

    return true;
}

static bool _vfs_clean_path(char *path_out) {

    // this function writes the cleaned up directory in place
    //  inside of path_out (usually scratch_pwd)
    //  it cleans repeated '/'s, ".", and ".."

    if (!path_out) return false;
    if (path_out[0] != '/') return false;

    // unecessary cleanout of seg_starts
    memset(seg_starts, 0x00, sizeof(seg_starts));
    uint16_t seg_top = 0;

    uint16_t write_pos = 0;
    path_out[write_pos++] = '/';

    // jump past first '/'
    const char *path_out_ptr = path_out + 1;
    while (*path_out_ptr) {

        // jump over any '/'s
        while(*path_out_ptr == '/') path_out_ptr += 1;
        if (!*path_out_ptr) break;

        // save pointer to first non-'/' char
        const char *segment = path_out_ptr;
        // blitz through all chars until next '/'
        while (*path_out_ptr && *path_out_ptr != '/') path_out_ptr += 1;
        // save length of these chars
        uint16_t seg_len = (path_out_ptr - segment);

        // if it's a ".", ignore it
        if (seg_len == 1 && segment[0] == '.') {
            continue;
        }
        // if it's a "..", move write pos to before current directory (saved in seg_starts)
        if (seg_len == 2 && segment[0] == '.' && segment[1] == '.') {
            if (seg_top > 0) {
                write_pos = seg_starts[--seg_top];
                if (write_pos == 0) { path_out[write_pos++] = '/'; }
            } else {
                write_pos = 1;
            }
            continue;
        }

        // write a '/' for this next dir
        if (write_pos > 1 && path_out[write_pos - 1] != '/') {
            if (write_pos + 1 >= VFS_MAX_PATH_LEN) return false;
            path_out[write_pos++] = '/';
        }
        if (write_pos + seg_len >= VFS_MAX_PATH_LEN) return false;

        // save this segment in seg_starts
        if (seg_top < (uint16_t)(VFS_MAX_PATH_LEN/2)) {
            seg_starts[seg_top++] = write_pos;
        } else {
            return false;
        }

        // write segment at write_pos
        memcpy(&path_out[write_pos], segment, seg_len);
        write_pos += (uint16_t)seg_len;
    }

    if (write_pos == 0) return false;
    
    // add trailing '/' if needed
    if (path_out[write_pos - 1] != '/') {
        if (write_pos + 1 >= VFS_MAX_PATH_LEN) return false;

        path_out[write_pos] = '/';
        path_out[write_pos + 1] = '\0';
    } else {
        path_out[write_pos] = '\0';
    }

    return true;
}

static bool _vfs_resolve_path(const char *path_in, char *abs_path_out) {

    if (!_vfs_build_abs_path(path_in, abs_path_out)) return false;
    if (!_vfs_clean_path(abs_path_out)) return false;

    return true;
}

static bool _vfs_valid_83_name(const char *path, uint16_t path_len) {

    if (path_len == 1 && path[0] == '/') {
        return true;
    }

    if (path[path_len - 1] == '/') path_len--;

    const char *name_start = path;
    for (int i = path_len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            name_start = &path[i + 1];
            break;
        }
    }

    uint16_t segment_len = (uint16_t)(&path[path_len] - name_start);
    if (segment_len == 0 || segment_len > 12) return false;

    // Check for illegal FAT16 characters
    static const char illegal[] = "\"*+,/:;<=>?[\\]|";
    for (uint16_t i = 0; i < segment_len; i++) {
        char c = name_start[i];
        if (c <= 0x20 || c == 0x7F) return false;
        if (memchr(illegal, c, sizeof(illegal) - 1)) return false;
    }

    const char *dot = memchr(name_start, '.', segment_len);

    if (dot) {
        uint16_t base_len = (uint16_t)(dot - name_start);
        uint16_t ext_len = (uint16_t)(&name_start[segment_len] - (dot + 1));

        if (base_len == 0 || base_len > 8 || ext_len > 3) return false;

        // Ensure only one dot
        if (memchr(dot + 1, '.', ext_len)) return false;
    } else {
        if (segment_len > 8) return false;
    }

    return true;
}

int32_t vfs_build_valid_path(const char *path_in, char *path_out, bool is_file) {

    if (!path_in || !path_in[0]) return -1;

    // VFS_MAX_PATH_LEN + 1 because path arrays are actually 256+1 long for nul terminator
    memset(path_out, 0x00, VFS_MAX_PATH_LEN + 1);

    if (!_vfs_resolve_path(path_in, path_out)) return -1;

    int32_t new_path_len = (int32_t)strnlen(path_out, VFS_MAX_PATH_LEN);
    if (new_path_len == 0 || new_path_len >= VFS_MAX_PATH_LEN) return -1;

    // capitalize all the a-z chars
    if (!_vfs_capitalize_path(path_out, (uint16_t)new_path_len)) return -1;

    if (is_file && new_path_len > 2) { // remove trailing '/' added by _vfs_resolve_path
        path_out[new_path_len-1] = '\0';
        new_path_len -= 1;
    }
    
    if (!_vfs_valid_83_name(path_out, (uint16_t)new_path_len)) return -1;

    return new_path_len;
}

/*************************************************
 *  vfs_getpwd()
*************************************************/

const char* vfs_getpwd(void) {

    return pwd;
}

/*************************************************
 *  vfs_open()
*************************************************/

file_desc_t vfs_open(const char *path, file_open_mode_t mode) {

    // === All the verifying and moving to get the scratch_pwd correct
    if (vfs_build_valid_path(path, scratch_pwd, true) < 0) return -1;

    // === Actually open the file
    // open the file with the dummy entry in the fd_table
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd, mode);

    // verify flags
    if ((fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        if (fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VOL_FULL) {
            debug_deposit("VFSL-CRIT-vfs_open(CREATE) failed due to VOL FULL flag", 0, DBG_NULL_VAR);
        }
        return -1;
    }

    // === check if file already open
    bool file_already_open = false;
    file_desc_t fd_index = 1;
    while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
        if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
            file_already_open = true;
            break;
        }
        fd_index += 1;
    }
    if (file_already_open && ((mode & FS_MODE_READ_ONLY) != 0)) { // file already open, but mode is READ_ONLY, so its fine
        // the READ_ONLY file will get it's own fd_table entry (due to offset usage)
        file_already_open = false;
    }
    
    if ((file_already_open && fd_table[fd_index].in_use)
            || ((fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_RO) && !(mode & FS_MODE_READ_ONLY))
            || ((fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_SYS) && !(mode & FS_MODE_FORCE))) {
        // return special value (0) that tells caller, you can try to open this again as READ_ONLY
        return 0;
    }

    // find empty entry (not in_use) in fd_table
    fd_index = 1;
    while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {

        if (!fd_table[fd_index].in_use) break;
        fd_index += 1;
    }
    if (fd_index == 8) { // table is full of "in_use" files (HOW??)
        return -1;
    }

    // move the scratch entry there and clean the STAGE entry
    memcpy(&fd_table[fd_index], &fd_table[VFS_FD_STAGE], sizeof(vfs_file_desc_t));
    memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));

    // prime the entry for use:
    fd_table[fd_index].in_use = true;
    if (mode & FS_MODE_APPEND) {
        fd_table[fd_index].offset = fd_table[fd_index].handle.file_size;
    } else {
        fd_table[fd_index].offset = 0;
    }
    
    // === return the fd
    return fd_index;
}

/*************************************************
 *  vfs_close()
*************************************************/

bool vfs_close(file_desc_t fd) {

    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    if (fd_table[fd].in_use == false) return false;

    bool return_val = fat_close(&fd_table[fd].handle);

    if (return_val) memset(&fd_table[fd], 0, sizeof(vfs_file_desc_t));

    return return_val;
}

/*************************************************
 *  vfs_seek() and its helpers
*************************************************/

bool vfs_seek(file_desc_t fd, int32_t offset, fs_offset_origin_t origin) {

    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    if (fd_table[fd].in_use == false) return false;

    uint32_t cur_offset = fd_table[fd].offset;
    uint32_t file_size = fd_table[fd].handle.file_size;
    uint32_t new_offset;

    switch (origin) {
        case FS_SEEK_START:
            if (offset < 0 || (uint32_t)offset > file_size) return false;
            new_offset = (uint32_t)offset;
            break;

        case FS_SEEK_CURRENT:
            if (offset > 0 && (uint32_t)offset > (file_size - cur_offset)) return false;
            if (offset < 0 && (uint32_t)(-offset) > cur_offset) return false;
            new_offset = cur_offset + offset;
            break;

        case FS_SEEK_END:
            if (offset > 0 || (uint32_t)(-offset) > file_size) return false;
            new_offset = file_size + offset;
            break;

        default: return false;
    } 

    fd_table[fd].offset = new_offset;
    return true;
}

/*************************************************
 *  vfs_tell() and its helpers
*************************************************/

uint32_t vfs_tell(file_desc_t fd) {

    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return -1;
    if (fd_table[fd].in_use == false) return -1;

    return fd_table[fd].offset;
}

/*************************************************
 *  vfs_read() and its helpers
*************************************************/

int32_t vfs_read(file_desc_t fd, uint8_t *dst, uint32_t len) {

    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return -1;
    if (fd_table[fd].in_use == false) return -1;
    // if (fd_table[fd].handle.flags & FS_MODE_READ_ONLY) return -1;

    printf("reading file of size: %u\n", fd_table[fd].handle.file_size);

    int32_t bytes_read = fat_read(&fd_table[fd].handle, fd_table[fd].offset, dst, len);

    if (bytes_read < 0) return -1;
    
    if (bytes_read >= 0 ) {
        fd_table[fd].offset += bytes_read;
    }
    printf("post read: new offset: %u\n", fd_table[fd].offset);

    return bytes_read;
}

/*************************************************
 *  vfs_write() and its helpers
*************************************************/

int32_t vfs_write(file_desc_t fd, uint8_t *src, uint32_t len) {

    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return -1;
    if (fd_table[fd].in_use == false) return -1;
    if (fd_table[fd].handle.flags & FS_MODE_READ_ONLY) {
        debug_deposit("VFSL-CRIT-Tried to vfs_write() to file opened in read only mode", 0, DBG_NULL_VAR);
        return -1;
    }

    if ((fd_table[fd].handle.attributes & FS_ATTR_SYS) 
            && !(fd_table[fd].handle.flags & FS_FORCE_FLAG)) {
        debug_deposit("VFSL-CRIT-Tried to vfs_write() to System file without forcing", 0, DBG_NULL_VAR);
        return -1;

    }

    printf("writing to file of size: %u\n", fd_table[fd].handle.file_size);

    int32_t bytes_written =  fat_write(&fd_table[fd].handle, fd_table[fd].offset, src, len);

    if (bytes_written < 0) return -1;

    if (bytes_written >= 0) {
        fd_table[fd].offset += bytes_written;
    }
    
    printf("post write: new offset: %u\n", fd_table[fd].offset);
    printf("post write: new file size: %u\n", fd_table[fd].handle.file_size);

    return bytes_written;
}

/*************************************************
 *  vfs_fsync() and its helpers
*************************************************/

bool vfs_fsync(file_desc_t fd) {
    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    if (fd_table[fd].in_use == false) return false;

    // does not close, so much as sync with disk
    return fat_close(&fd_table[fd].handle);
}

/*************************************************
 *  vfs_truncate() and its helpers
*************************************************/

bool vfs_truncate(file_desc_t fd, uint32_t new_size) {
    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    if (fd_table[fd].in_use == false) return false;
    if (fd_table[fd].handle.flags & FS_MODE_READ_ONLY) {
        debug_deposit("VFSL-CRIT-Tried to vfs_truncate() file opened in read only mode", 0, DBG_NULL_VAR);
        return false;
    }

    if ((fd_table[fd].handle.attributes & FS_ATTR_SYS) 
            && !(fd_table[fd].handle.flags & FS_FORCE_FLAG)) {
        debug_deposit("VFSL-CRIT-Tried to vfs_truncate() to System file without forcing", 0, DBG_NULL_VAR);
        return -1;

    }

    printf("fd passed truncate fd tests\n");

    if (fat_truncxtend(&fd_table[fd].handle, new_size)) {

        // clamp the fd offset to the new size
        if (new_size < fd_table[fd].offset) {
            fd_table[fd].offset = new_size;
        }

        return true;
    } else {
        return false;
    }
}

/*************************************************
 *  vfs_stats functions & vfs_timestamp_struct()
*************************************************/

// typedef struct {
//     uint8_t create_time[4]; 
//     uint8_t modify_time[4]; 
//     uint32_t file_size; 
//     uint8_t attribute;
// } fs_out_stat_t;

bool vfs_stat_path(const char *path, fs_out_stat_t *stats) {
    if (stats == NULL) return false;

    memset(stats, 0, sizeof(fs_out_stat_t));

    if (vfs_build_valid_path(path, scratch_pwd, true) < 0) return false;

    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd, FS_MODE_READ_ONLY);

    // verify flag
    if ((fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        debug_deposit("VFSL-WARN-Tried to get stats for an invalid path (does it exist?)", 0, DBG_NULL_VAR);
        return false;
    }

    stats->file_size = fd_table[VFS_FD_STAGE].handle.file_size;
    stats->attribute = fd_table[VFS_FD_STAGE].handle.attributes;

    if (!fat_timestamp_bytes(&fd_table[VFS_FD_STAGE].handle, FAT_TIMESTAMP_CREATION, stats->create_time)) {
        debug_deposit("VFSL-CRIT-FAT found dir cache invalid when reading directory for create timestam", 0, DBG_NULL_VAR);
        return false;
    }

    if (!fat_timestamp_bytes(&fd_table[VFS_FD_STAGE].handle, FAT_TIMESTAMP_LAST_WRITE, stats->modify_time)) {
        debug_deposit("VFSL-CRIT-FAT found dir cache invalid when reading directory for mod timestamp", 0, DBG_NULL_VAR);
        return false;
    }

    memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));

    return true;
}

bool vfs_timestamp_tm(uint8_t *ts_bytes, struct tm *timestamp) {

    if (ts_bytes[0] || ts_bytes[1] || ts_bytes[2] || ts_bytes[3]) { // timestamp is non-zero
        fat_timestamp_tm(ts_bytes, timestamp);
        return true;
    } else { // timestamp is just zeros, nothing here
        return false;
    }
}

bool vfs_stat_fd(file_desc_t fd, fs_out_stat_t *stats) {
    if (stats == NULL) return false;
    if (fd < 0 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    // fd checks here in vfs_stat_fd are weaker because vfs_listdir() calls this
    //      to get timestamps and it uses fd_table[VFS_FD_STAGE]

    printf("fd passed vfs_stat_fd test\n");

    memset(stats, 0, sizeof(fs_out_stat_t));

    stats->file_size = fd_table[fd].handle.file_size;
    stats->attribute = fd_table[fd].handle.attributes;

    printf("file_size: %u\n", fd_table[fd].handle.file_size);
    printf("attribute byte: %02X\n", fd_table[fd].handle.attributes);

    if (!fat_timestamp_bytes(&fd_table[fd].handle, FAT_TIMESTAMP_CREATION, stats->create_time)) {
        debug_deposit("VFSL-CRIT-FAT found dir cache invalid when reading directory for create timestam", 0, DBG_NULL_VAR);
        return false;
    }

    printf("Creation timestamp bytes:\n");
    for (uint8_t i = 0; i < 4; i++) {
        printf("%u: %02X\n", i, stats->create_time[i]);
    }

    if (!fat_timestamp_bytes(&fd_table[fd].handle, FAT_TIMESTAMP_LAST_WRITE, stats->modify_time)) {
        debug_deposit("VFSL-CRIT-FAT found dir cache invalid when reading directory for mod timestamp", 0, DBG_NULL_VAR);
        return false;
    }

    printf("Modify timestamp bytes:\n");
    for (uint8_t i = 0; i < 4; i++) {
        printf("%u: %02X\n", i, stats->modify_time[i]);
    }

    return true;
}

/*************************************************
 *  vfs_get_file_name() & file_size()
*************************************************/

uint32_t vfs_get_file_size(file_desc_t fd) {
    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return false;
    if (fd_table[fd].in_use == false) return false;

    return fd_table[fd].handle.file_size;
}

char * vfs_get_file_name(file_desc_t fd) {
    if (fd < 1 || fd > VFS_NUM_FILE_DESCRIPTIONS) return NULL;
    if (fd_table[fd].in_use == false) return NULL;

    return fd_table[fd].handle.file_name;
}

/*************************************************
 *  vfs_fsync() and its helpers
*************************************************/

bool vfs_delete(const char *path, bool force) {

    // === All the verifying and moving to get the scratch_pwd correct
    if (vfs_build_valid_path(path, scratch_pwd, true) < 0) return false;

    printf("vfs file path:");
    printf(scratch_pwd);
    printf("\n");

    // === Actually open the file
    // open the file with the dummy entry in the fd_table
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd, FS_MODE_WRITE);

    // verify flags
    if ((fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        return false;
    }

    if ((fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_SYS) && !force) {
        debug_deposit("VFSL-CRIT-Tried to vfs_delete() System file without forcing", 0, DBG_NULL_VAR);
        return false;
    }

    // === check if file already open
    bool file_already_open = false;
    file_desc_t fd_index = 1;
    while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
        if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
            file_already_open = true;
            break;
        }
        fd_index += 1;
    }
    
    if (file_already_open && fd_table[fd_index].in_use) { // it is open but is it in use?
        debug_deposit("VFSL-CRIT-Cannot delete() file. File in use in fd_table. index: ", fd_index, DBG_U32_DEC);
        return false; // file in use, do not delete
    }

    memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
    
    return fat_delete(scratch_pwd);
}

/*************************************************
 *  vfs_mkdir() & vfs_rmdir()
*************************************************/

bool vfs_mkdir(const char *path) {

    // === All the verifying and moving to get the scratch_pwd correct
    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    printf("vfs file path:");
    printf(scratch_pwd);
    printf("\n");

    printf("path made successfully\n");

    return fat_mkdir(scratch_pwd);
}

bool vfs_rmdir(const char *path, bool force) {

    // === All the verifying and moving to get the scratch_pwd correct
    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    fat_file_handle_t dir_handle = {0};
    if (!fat_find_entry(scratch_pwd, &dir_handle)) return false;

    if ((dir_handle.attributes & FS_ATTR_SYS) && !force) {
        debug_deposit("VFSL-CRIT-Tried to vfs_rmdir() System directory without forcing", 0, DBG_NULL_VAR);
        return false;
    }

    return fat_rmdir(scratch_pwd);
}

/*************************************************
 *  vfs_move() & vfs_copy() and their helpers
*************************************************/

bool vfs_move(const char *src_path, const char *dst_path, bool force) {

    // === validate both paths:
    if (vfs_build_valid_path(src_path, scratch_pwd, true) < 0) return false;

    // this is just to find dir_entry address for comparing with fd_table entries
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd, FS_MODE_READ_ONLY); 

    if ((fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        return false; // source file was not found in drive.
    }

    if ((fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_SYS) && !force) {
        debug_deposit("VFSL-CRIT-Tried to vfs_move() System file without forcing", 0, DBG_NULL_VAR);
        return false;
    }

    // if src is in use, we cannot move/rename it
    bool file_already_open = false;
    file_desc_t fd_index = 1;
    while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
        if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
            file_already_open = true;
            break;
        }
        fd_index += 1;
    }
    memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
    
    if (file_already_open && fd_table[fd_index].in_use) {
        return false; // if src is in use, we cannot move/rename it
    }

    // === dst file must NOT be in use, if it exists
    if (vfs_build_valid_path(dst_path, scratch_pwd2, true) < 0) return false;

    // this is just to find dir_entry address for comparing with fd_table entries
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd2, FS_MODE_READ_ONLY); 

    if (fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) { // file exists on drive
        // this is okay, but we need to make sure the file is not in use in VFS

        file_already_open = false;
        fd_index = 1;
        while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
            if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
                file_already_open = true;
                break;
            }
            fd_index += 1;
        }
        memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
        
        if (file_already_open && fd_table[fd_index].in_use) {
            return false;
        }
    }

    // === Now we pass the two cleaned paths (scratch_pwd & scratch_pwd2) to fat_move()
    //  it will verify src file exsts and create dst if it does not exist
    return fat_move(scratch_pwd, scratch_pwd2);
}


bool vfs_copy(const char *src_path, const char *dst_path) {

    // === validate both paths:
    if (vfs_build_valid_path(src_path, scratch_pwd, true) < 0) return false;

    // this is just to find dir_entry address for comparing with fd_table entries
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd, FS_MODE_READ_ONLY); 

    if ((fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        return false; // source file was not found in drive.
    }

    //  if src is in use, vfs_fsync() will flush it to storage for copying
    bool file_already_open = false;
    file_desc_t fd_index = 1;
    while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
        if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
            file_already_open = true;
            break;
        }
        fd_index += 1;
    }
    memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
    
    if (file_already_open && fd_table[fd_index].in_use) {
        // syncs all file details to disk
        if (!vfs_fsync(fd_index)) return false;
    }
    

    // === dst file must NOT be in use, if it exists
    if (vfs_build_valid_path(dst_path, scratch_pwd2, true) < 0) return false;

    // this is just to find dir_entry address for comparing with fd_table entries
    fd_table[VFS_FD_STAGE].handle = fat_open(scratch_pwd2, FS_MODE_READ_ONLY); 

    if (fd_table[VFS_FD_STAGE].handle.flags & FAT_HANDLE_FLAG_VALID) { // file exists on drive
        // this is okay, but we need to make sure the file is not in use in VFS

        file_already_open = false;
        fd_index = 1;
        while (fd_index < VFS_NUM_FILE_DESCRIPTIONS) {
            if (fd_table[VFS_FD_STAGE].handle.dir_entry == fd_table[fd_index].handle.dir_entry) {
                file_already_open = true;
                break;
            }
            fd_index += 1;
        }
        memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
        
        if (file_already_open && fd_table[fd_index].in_use) {
            return false;
        }
    }

    // === Now we pass the two cleaned paths (scratch_pwd & scratch_pwd2) to fat_copy()
    //  it will verify src file exsts and create dst if it does not exist
    return fat_copy(scratch_pwd, scratch_pwd2);
}

/*************************************************
 *  vfs_exists()
*************************************************/

bool vfs_exists(const char *path) {

    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    fat_file_handle_t item_handle = {0};
    return fat_find_entry(scratch_pwd, &item_handle);
}

bool vfs_is_dir(const char *path) {

    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    fat_file_handle_t item_handle = {0};
    bool found_item = fat_find_entry(scratch_pwd, &item_handle);

    bool is_dir = item_handle.attributes & FS_ATTR_DIR;

    return (found_item && is_dir);
}

fs_error_flag_t vfs_errno(file_desc_t fd) {
    // do not use. fd_table entries are wiped after an error, so handle flags
    //  for VOL FUL or INVALID never stick around to be picked up by this function
    //      if you want this: move all fd_table[VFS_FD_STAGE] wipes to BEFORE fat_open() calls
    //      then on error, you can read the last handle grabbed at fd = VFS_FD_STAGE
    // but I don't see a need for this now
}

/*************************************************
 *  vfs_opendir() & vfs_listdir() & helpers
*************************************************/

bool vfs_opendir(const char *path, fat_dir_list_t *dir) {

    // verifying and moving to get the scratch_pwd correct
    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    int32_t count = fat_opendir(scratch_pwd, false, dir);
    if (count < 0) return false;

    return true;
}

bool vfs_listdir(fat_dir_list_t *dir, fs_file_fields_t fields, vfs_dir_entry_t *entry_out, bool all_flag) {

    if (dir->index >= dir->num_entries) {
        return false;
    }

    memset(entry_out, 0x00, sizeof(vfs_dir_entry_t));

    while (1) {
        if (!fat_listdir(dir, &fd_table[VFS_FD_STAGE].handle)) return false;
        printf("Attr Byte: 0x%02X\n", fd_table[VFS_FD_STAGE].handle.attributes);

        dir->index += 1;

        if (fd_table[VFS_FD_STAGE].handle.file_name[0] == 0xE5 // filter deleted entries
            || fd_table[VFS_FD_STAGE].handle.attributes == 0x0F // filter LFN entry
            || fd_table[VFS_FD_STAGE].handle.file_name[6] == '~' // filter LFN alias entry
            || ((fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_HID) && (!all_flag)) // filter hidden entry
            || fd_table[VFS_FD_STAGE].handle.attributes & FS_ATTR_VOL ) { // filter volume name
                continue;
        } else {
            printf("not filtered entry. Parsing...\n");
            // all the actual parsing is done in here
            bool return_val = _vfs_parse_fd(VFS_FD_STAGE, fields, entry_out);

            printf("entry name:");
            printf(entry_out->name);
            printf("\n");

            memset(&fd_table[VFS_FD_STAGE], 0, sizeof(vfs_file_desc_t));
            return return_val;
        }
    }
}

bool _vfs_parse_fd(file_desc_t fd, fs_file_fields_t fields, vfs_dir_entry_t *entry) {

    uint8_t string_idx = 0;

    // == FILE NAME - always parsed
    // fills with spaces for padding when printing
    memset(entry->name, ' ', VFS_FILE_NAME_LEN); // fixed 13 size: 8.3 + '.' and/or '/'

    // parse the name
    for (uint8_t i = 0; i < 8; i++) { // 8 for 8.3 FAT16 file naming convention
        if (fd_table[fd].handle.file_name[i] != ' ') {
            entry->name[string_idx] = fd_table[fd].handle.file_name[i];
            string_idx += 1;
        }
    }

    // transfer extension and '.'(?)
    if (fd_table[fd].handle.file_name[8] != ' ') {
        entry->name[string_idx] = '.';
        string_idx += 1;

        for (uint8_t i = 8; i < 11; i++) { // 3 for 8.3 FAT16 file naming convention
            if (fd_table[fd].handle.file_name[i] != ' ') {
                entry->name[string_idx] = fd_table[fd].handle.file_name[i];
                string_idx += 1;
            }
        }
    }

    // if entry is a directory, add "/" at end to denote it
    if ((fd_table[fd].handle.attributes & FS_ATTR_DIR) // directory attribute bit
            // && (fd_table[fd].handle.file_name[8] == ' ')
        ) { // and a '.' was not added, else we clobber \0
        entry->name[string_idx] = '/';
        string_idx += 1;
    }

    // fill in entry->size if FILE_SIZE is requested
    if (fields == FS_FILE_NAME_SIZE_MTIME_ATTR) {

        if ((fd_table[fd].handle.attributes & 0x10) == 0) { // if not a directory (0x10 dir attribute)

            uint32_t file_size_kB = fd_table[fd].handle.file_size / 1024; // convert to kB
            file_size_kB += (fd_table[fd].handle.file_size % 1024 != 0); // round up

            sprintf(entry->size, "%7ukB", file_size_kB);

        } else { // directories don't have a size
            memset(entry->size, ' ', VFS_FILE_SIZE_LEN);
        }
    }

    // fill in entry->ts if FILE_MTIME is requested
    if (fields == FS_FILE_NAME_SIZE_MTIME_ATTR) {

        memset(entry->ts, ' ', VFS_FILE_TS_LEN); // length of timestamp string is 16 chars

        fs_out_stat_t stats;
        // vfs_stat_fd() for VFS_FD_STAGE index to get stats struct
        if (!vfs_stat_fd(fd, &stats)) return false;

        struct tm timestamp;
        if (vfs_timestamp_tm(stats.modify_time, &timestamp)) {

            sprintf(entry->ts, "%04d-%02d-%02d %02d:%02d", 
                timestamp.tm_year + 1900, 
                timestamp.tm_mon + 1, 
                timestamp.tm_mday, 
                timestamp.tm_hour, 
                timestamp.tm_min);

        } else { // vfs_timestamp_tm() returns false if timestamp is empty/zerod
            // do nothing, keep entry->ts empty
        }
    }

    if (fields == FS_FILE_NAME_SIZE_MTIME_ATTR) {
        memset(entry->attr, '.', VFS_FILE_ATTR_LEN);

        if (fd_table[fd].handle.attributes & FS_ATTR_SYS) {
            entry->attr[0] = 'S';
        }
        if (fd_table[fd].handle.attributes & FS_ATTR_HID) {
            entry->attr[1] = 'H';
        }
        if (fd_table[fd].handle.attributes & FS_ATTR_RO) {
            entry->attr[2] = 'R';
        }
    }
    return true;
}



/*************************************************
 *  Entry Attribute Modifications
*************************************************/

bool vfs_get_attr(const char *path, uint8_t *attr) {
    
    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    return fat_get_attr(scratch_pwd, attr);
}

bool vfs_set_attr(const char *path, uint8_t attr) {

    if (vfs_build_valid_path(path, scratch_pwd, false) < 0) return false;

    return fat_set_attr(scratch_pwd, attr);
}
