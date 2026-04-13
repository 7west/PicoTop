#pragma once

// There are too many #defines to put in fat16.c, so they get their own file



// MBR DEFINES
#define MBR_SECT_ADDR 0x00
#define MBR_PARTITION1 0x01BE
#define MBR_SYSTEMID_OFFSET 0x04
#define MBR_RELSECT_OFFSET 0x08
#define MBR_TOTSECT_OFFSET 0x0C //12
#define MBR_SECT_SIG_OFFSET 0x01FE

// FAT16 VBR DEFINES
#define FAT16_BPS_OFFSET 0x000B //bytes per sector
#define FAT16_SPC_OFFSET 0x000D //sectors per cluster
#define FAT16_RES_SECT_OFFSET 0x000E //# of reserved sectors (VBR+FATs)
#define FAT16_NUM_FAT_OFFSET 0x0010 //number of FATs
#define FAT16_ROOT_ENTRIES_OFFSET 0x0011 //number of root dir entries
#define FAT16_NUM_SECT_SM_OFFSET 0x0013 // num of sectors, for small drives only (<32Mb)
#define FAT16_SPFAT_OFFSET 0x0016 // sectors per FAT
#define FAT16_HIDDEN_SECT_OFFSET 0x001C // number of hidden sectors preceding this VBR
#define FAT16_NUM_SECT_LG_OFFSET 0x0020 // number of sectors, for large drives
#define FAT16_VOL_LABEL_OFFSET 0x002B // 11 characters
#define FAT16_FORMAT_TYPE_OFFSET 0x0036 // should say "FAT16   "
#define FAT16_MEDIA_DESC_OFFSET 0x0015
#define FAT16_DRIVE_NUM_OFFSET 0x0024
#define FAT16_EXT_BOOT_SIG_OFFSET 0x0026
#define FAT16_VOL_SN_OFFSET 0x0027
#define FAT16_OEM_ID_OFFSET 0x0003

// FAT16 DIRECTORY ENTRY DEFINES
#define DIR_ENTRY_SIZE 0x20
#define DIR_FILENAME_OFFSET 0x00
#define DIR_EXTNAME_OFFSET 0x08
#define DIR_ATTR_OFFSET 0x0B
#define DIR_CREATE_TIME_OFFSET 0x0E
#define DIR_ACCESS_TIME_OFFSET 0x12
#define DIR_WRITE_TIME_OFFSET 0x16
#define DIR_START_CLUSTER_OFFSET 0x1A
#define DIR_FILE_SIZE_OFFSET 0x1C
#define DIR_DELETED_ENTRY 0xE5


// FAT16 MISC DEFINES
#define FAT_VBR_START_SECTOR 0x80 // 128 sectors
#define FAT_NUM_ROOT_ENTRIES 512
#define FAT_RESV_SECT 8 // because that seems fine to me
#define FAT_NUM_FATS 2