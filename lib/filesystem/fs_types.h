#pragma once


#define FAT_SPARE_BUF_SIZE 256
#define VFS_MAX_PATH_LEN FAT_SPARE_BUF_SIZE

#define FS_READ_ONLY_FLAG 0x01
#define FS_WRITE_FLAG 0x02
#define FS_APPEND_FLAG 0x04
#define FS_CREATE_FLAG 0x08
#define FS_FORCE_FLAG 0x10
// blank for now - 0x20
#define FS_VOL_FULL_FLAG 0x40
#define FS_VALID_FLAG 0x80

#define FS_ATTR_STD 0x20
#define FS_ATTR_RO 0x01
#define FS_ATTR_HID 0x02
#define FS_ATTR_SYS 0x04
#define FS_ATTR_VOL 0x08
#define FS_ATTR_DIR 0x10

#define FAT_8_3_NAME 8
#define FAT_8_3_EXT 3
#define FAT_8_3_FULL (FAT_8_3_NAME + FAT_8_3_EXT + 1) // +1 for the "."

typedef enum {
    FS_MODE_READ_ONLY = FS_READ_ONLY_FLAG,
    FS_MODE_WRITE = FS_WRITE_FLAG,
    FS_MODE_APPEND = FS_APPEND_FLAG,
    FS_MODE_CREATE = FS_CREATE_FLAG,
    FS_MODE_FORCE = FS_FORCE_FLAG,
} file_open_mode_t;

typedef struct {
    uint16_t list_dir_sector;
    uint16_t index;
    uint16_t num_entries;
} fat_dir_list_t;


