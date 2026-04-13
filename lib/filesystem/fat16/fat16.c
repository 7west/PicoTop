
#include <string.h>

#include "fat16.h"
#include "os_debug.h"
#include "fat16_defines.h"
#include "rtc.h"
#include "sd_block.h"
#include "info_doc.h"


// DEFINES
//  see fat16_defines.h
#define printf(...) ((void)0)

// CACHES
static uint32_t gen_cache_lba = 0xFFFFFFFF;
static bool gen_cache_dirty = false;
bool gen_cache_valid = true;
static uint8_t gen_cache[SD_BLOCK_SIZE] = {0};

static uint32_t dir_cache_lba = 0xFFFFFFFF;
static bool dir_cache_dirty = false;
bool dir_cache_valid = true;
static uint8_t dir_cache[SD_BLOCK_SIZE] = {0};

static uint32_t fat_cache_lba = 0xFFFFFFFF;
static bool fat_cache_dirty = false;
bool fat_cache_valid = true;
static uint8_t fat_cache[SD_BLOCK_SIZE] = {0};

static uint8_t spare_buf[FAT_SPARE_BUF_SIZE];

static bool sd_mounted = false;


// persistent variables
static bool first_boot = true;
static uint32_t VolumeStart;
static uint16_t BytesPerSector;
static uint16_t SectorsPerCluster;
static uint32_t BytesPerCluster;
static uint16_t SectorsPerFAT;
static uint32_t TotalSectors;
static uint8_t NumOfFATs;
static uint16_t NumRootEntries;
static char VolumeLabel[11];
static uint16_t NumFATEntries;

// all in [sectors]
static uint32_t ReservedRegionStart;
static uint32_t ReservedRegionSize = 0; //acts as test for proper initialization throughout library
static uint32_t FATRegionStart;
static uint32_t FATRegionSize;
static uint32_t RootDirRegionStart;
static uint32_t RootDirRegionSize;
static uint32_t DataRegionStart;
static uint32_t DataRegionSize;
static uint16_t IndicesPerSector;

// TODO: some check of directories when a path has a folder that doesn't exist
//          don't make the folder, but tell fat_open/VFS/user it doesn't exist



bool fat_mount_sd(bool sd_soft_init) {

    if (sd_mounted) {
        debug_deposit("FT16-CRIT-SD already marked as 'mounted'", 0, DBG_NULL_VAR);
        return false;
    }

    if (first_boot) {
        sd_block_boot_init();
        first_boot = false;
    }
    
    if (sd_soft_init) {
        if (!sd_block_soft_init()) {
            debug_deposit("FT16-CRIT-SD init failed. FAT16 init halted", 0, DBG_NULL_VAR);
            return false;
        }
    }

    // read lba0 for MBR
    if (!sd_read_sectors(0, 1, gen_cache)) {
        debug_deposit("FT16-CRIT-MBR read failed. FAT16 init halted", 0, DBG_NULL_VAR);
        return false;
    }

    printf("MBR:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    // parse MBR for partition 1:
    //  System ID
    //  Start sector
    //  total sectors
    if (gen_cache[MBR_SECT_SIG_OFFSET] != 0x55 || gen_cache[MBR_SECT_SIG_OFFSET+1] != 0xAA) {
        debug_deposit("FT16-CRIT-MBR is improperly formatted, no 0x55AA", 0, DBG_NULL_VAR);
        return false;
    }

    // check for FAT16 formatting at System ID (0x0E)
    if (gen_cache[MBR_PARTITION1 + MBR_SYSTEMID_OFFSET] != 0x0E) {
        debug_deposit("FT16-CRIT-Partition 1 is NOT FAT16. SystemID = ", 
            gen_cache[MBR_PARTITION1 + MBR_SYSTEMID_OFFSET], DBG_U8_HEX);
        return false;
    }

    VolumeStart = _le32p(&gen_cache[MBR_PARTITION1 + MBR_RELSECT_OFFSET]);
    TotalSectors = _le32p(&gen_cache[MBR_PARTITION1 + MBR_TOTSECT_OFFSET]);

    printf("VolumeStart = 0x%08lX\n", VolumeStart);
    printf("TotalSectors = 0x%08lX\n", TotalSectors);

    // read lba### for VBR
    if (!sd_read_sectors(VolumeStart, 1, gen_cache)) {
        debug_deposit("FT16-CRIT-FAT16 VBR read failed. FAT16 relative sector: ", VolumeStart, DBG_U32_HEX);
        return false;
    }

    printf("\nFAT16 VBR:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }
    
    // verify MBR and VBR match well enough. This is probably a little overkill
    if (gen_cache[MBR_SECT_SIG_OFFSET] != 0x55 || gen_cache[MBR_SECT_SIG_OFFSET+1] != 0xAA) {
        debug_deposit("FT16-CRIT-VBR is improperly formatted, no 0x55AA", 0, DBG_NULL_VAR);
        return false;
    }
    uint32_t TempTotalSectors = _le16p(&gen_cache[FAT16_NUM_SECT_SM_OFFSET]);
    if (TempTotalSectors) {
        if (TotalSectors != TempTotalSectors) {
            debug_deposit("FT16-CRIT-16b Volume sector size mismatch MBR vs VBR. VBR: ", 
                TempTotalSectors, DBG_U32_HEX);
            return false;
        }
    } else {
        if (TotalSectors != _le32p(&gen_cache[FAT16_NUM_SECT_LG_OFFSET])) {
            debug_deposit("FT16-CRIT-Volume sector size mismatch MBR vs VBR. VBR: ", 
                _le32p(&gen_cache[FAT16_NUM_SECT_LG_OFFSET]), DBG_U32_HEX);
            return false;
        }
    }
    
    if (VolumeStart != _le32p(&gen_cache[FAT16_HIDDEN_SECT_OFFSET])) {
        debug_deposit("FT16-CRIT-Hidden sector size mismatch MBR vs VBR. VBR: ", 
            _le32p(&gen_cache[FAT16_NUM_SECT_LG_OFFSET]), DBG_U32_HEX);
        return false;
    }

    // load important values into statics
    BytesPerSector = _le16p(&gen_cache[FAT16_BPS_OFFSET]);
    if (BytesPerSector != SD_BLOCK_SIZE) {
        debug_deposit("FT16-CRIT-bytes/sector != 512. OS cannot handle this for now. BPS: ", BytesPerSector, DBG_U32_DEC);
        return false;
    }
    SectorsPerCluster = gen_cache[FAT16_SPC_OFFSET];
    BytesPerCluster = BytesPerSector * SectorsPerCluster;
    NumOfFATs = gen_cache[FAT16_NUM_FAT_OFFSET];
    NumRootEntries = _le16p(&gen_cache[FAT16_ROOT_ENTRIES_OFFSET]);
    SectorsPerFAT = _le16p(&gen_cache[FAT16_SPFAT_OFFSET]);
    // memcpy(VolumeLabel, &gen_cache[FAT16_VOL_LABEL_OFFSET], 11);
    IndicesPerSector = BytesPerSector / DIR_ENTRY_SIZE;

    printf("\nImportant Statics:\n");
    printf("BytesPerSector = %u\n", BytesPerSector);
    printf("SectorsPerCluster = %u\n", SectorsPerCluster);
    printf("NumOfFATs = %u\n", NumOfFATs);
    printf("NumRootEntries = %u\n", NumRootEntries);
    printf("SectorsPerFAT = %u\n", SectorsPerFAT);
    
    printf("IndicesPerSector = %u\n", IndicesPerSector);

    // all in [sectors]
    ReservedRegionStart = VolumeStart;
    ReservedRegionSize = _le16p(&gen_cache[FAT16_RES_SECT_OFFSET]);
    FATRegionStart = ReservedRegionStart + ReservedRegionSize;
    FATRegionSize = NumOfFATs * SectorsPerFAT;
    RootDirRegionStart = FATRegionStart + FATRegionSize;
    RootDirRegionSize = ((NumRootEntries * 32) + BytesPerSector - 1) / BytesPerSector;
    DataRegionStart = RootDirRegionStart + RootDirRegionSize;
    DataRegionSize = TotalSectors - (ReservedRegionSize + FATRegionSize + RootDirRegionSize);

    // at this point it is safe to say the drive is mounted properly
    // The rest is printing status for debugging (and getting the Volume Label)
    sd_mounted = true;

    printf("\nImportant Calculations:\n");
    printf("ReservedRegionStart = 0x%08lX\n", ReservedRegionStart);
    printf("ReservedRegionSize = 0x%08lX\n", ReservedRegionSize);
    printf("FATRegionStart = 0x%08lX\n", FATRegionStart);
    printf("FATRegionSize = 0x%08lX\n", FATRegionSize);
    printf("RootDirRegionStart = 0x%08lX\n", RootDirRegionStart);
    printf("RootDirRegionSize = 0x%08lX\n", RootDirRegionSize);
    printf("DataRegionStart = 0x%08lX\n", DataRegionStart);
    printf("DataRegionSize = 0x%08lX\n", DataRegionSize);

    memcpy(VolumeLabel, _load_dir_cache(RootDirRegionStart), 11);
    printf("Volume Label: %.*s\n", 11, VolumeLabel);
    NumFATEntries = (DataRegionSize + SectorsPerCluster - 1) / SectorsPerCluster;
    printf("NumFATEntries = %u\n", NumFATEntries);


    printf("\n\nFAT 1st Sector:\n");
    sd_read_sectors(FATRegionStart, 1, gen_cache);
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    printf("\n\nRoot Directory 1st Sector:\n");
    sd_read_sectors(RootDirRegionStart, 1, gen_cache);
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    printf("\n~~~~~~~~~~~~~~~~\n");

    debug_deposit("FT16-INFO-FAT16 File System mounted successfully. Cap [B]: ", DataRegionSize * SD_BLOCK_SIZE, DBG_U32_DEC);

    // ensures that read() functions reload caches
    gen_cache_lba = 0xFFFFFFFF;
    dir_cache_lba = 0xFFFFFFFF;
    fat_cache_lba = 0xFFFFFFFF;
    return true;
}

/*************************************************
 *  fat_format_sd() and its helpers
*************************************************/

bool fat_format_sd(void) {

    if (!sd_block_soft_init()) {
        debug_deposit("FT16-CRIT-SD init failed. Formatting of SD halted", 0, DBG_NULL_VAR);
        return false;
    }

    // Formatter cannot handle SD's smaller than 32Mb
    if (sd_get_sector_count() < 0xFFFF) {
        debug_deposit("FT16-CRIT-Formatter cannot process SD with sectors < 65536. sector: ", sd_get_sector_count(), DBG_U32_DEC);
        return false;
    }

    //=====MBR
    // zero MBR
    memset(gen_cache, 0x00, SD_BLOCK_SIZE);

    // Fill MBR with one partition entry (0x1BE)
    VolumeStart = FAT_VBR_START_SECTOR;
    TotalSectors = sd_get_sector_count() - sd_get_tail_sectors() - FAT_VBR_START_SECTOR;
    printf("Total Sectors for Partition 1: 0x%08lX\n", TotalSectors);
    _mbr_partition_entry(VolumeStart, TotalSectors, gen_cache + MBR_PARTITION1);

    // put 0x55AA
    gen_cache[0x1FE] = 0x55;
    gen_cache[0x1FF] = 0xAA;


    printf("\nMBR to be written:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    if (!sd_write_sectors(MBR_SECT_ADDR, 1, gen_cache)) {
        debug_deposit("FT16-CRIT-fat_format_sd() failed to write blocks. Formatting halted", 0, DBG_NULL_VAR);
    }

    //=====VBR
    // zero VBR
    memset(gen_cache, 0x00, SD_BLOCK_SIZE);

    //--calculate everything
    // Known constants
    BytesPerSector = SD_BLOCK_SIZE;
    ReservedRegionSize = FAT_RESV_SECT;
    ReservedRegionStart = VolumeStart;
    NumOfFATs = FAT_NUM_FATS;
    FATRegionStart = ReservedRegionStart + ReservedRegionSize;
    NumRootEntries = FAT_NUM_ROOT_ENTRIES;
    RootDirRegionSize = ((NumRootEntries * 32) + BytesPerSector - 1) / BytesPerSector;

    SectorsPerCluster = 1;
    uint32_t num_clusters = 0xFFFF + 1;
    while (num_clusters > (0xFFFF)) {
        SectorsPerCluster = SectorsPerCluster * 2;
        
        num_clusters = (TotalSectors - ReservedRegionSize - RootDirRegionSize) / SectorsPerCluster;
        //  This is technically wrong because FAT will take away TotalSectors as well
        //      but this gets us very close and is fine
        //      I just don't want to do a bunch of iterating...
    }

    uint32_t FAT_size_bytes = (num_clusters + 2) * 2;
    SectorsPerFAT = (FAT_size_bytes + BytesPerSector - 1) / BytesPerSector;
    FATRegionSize = NumOfFATs * SectorsPerFAT;
    RootDirRegionStart = FATRegionStart + FATRegionSize;
    DataRegionStart = RootDirRegionStart + RootDirRegionSize;

    // fill in 4x16 byte array
    memcpy(gen_cache + 0, (uint8_t[]){0xEB, 0x3C, 0x90}, 3);
    memcpy(gen_cache + FAT16_OEM_ID_OFFSET, "WESTOS.1", 8);
    w_le16(gen_cache, FAT16_BPS_OFFSET, BytesPerSector);
    gen_cache[FAT16_SPC_OFFSET] = SectorsPerCluster;
    w_le16(gen_cache, FAT16_RES_SECT_OFFSET, ReservedRegionSize);
    gen_cache[FAT16_NUM_FAT_OFFSET] = NumOfFATs;
    w_le16(gen_cache, FAT16_ROOT_ENTRIES_OFFSET, NumRootEntries);
    gen_cache[FAT16_MEDIA_DESC_OFFSET] = 0xF8;
    w_le16(gen_cache, FAT16_SPFAT_OFFSET, SectorsPerFAT);
    w_le32(gen_cache, FAT16_HIDDEN_SECT_OFFSET, FAT_VBR_START_SECTOR);
    w_le32(gen_cache, FAT16_NUM_SECT_LG_OFFSET, TotalSectors);
    gen_cache[FAT16_DRIVE_NUM_OFFSET] = 0x80;
    gen_cache[FAT16_EXT_BOOT_SIG_OFFSET] = 0x29;
    w_le32(gen_cache, FAT16_VOL_SN_OFFSET, 0x7A7A7A7A);
    memcpy(gen_cache + FAT16_VOL_LABEL_OFFSET, "WEST OS SD", 11);
    memcpy(gen_cache + FAT16_FORMAT_TYPE_OFFSET, "FAT16   ", 8);

    // write array & 0x55AA
    gen_cache[0x1FE] = 0x55;
    gen_cache[0x1FF] = 0xAA;

    printf("\nFAT16 VBR to be written:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    sd_write_sectors(VolumeStart, 1, gen_cache);

    //=====FAT
    // zero both FATs (entirely)
    memset(gen_cache, 0x00, SD_BLOCK_SIZE);
    for (uint32_t i = FATRegionStart; i < RootDirRegionStart; i++) {
        sd_write_sectors(i, 1, gen_cache);
    }

    // populate with first entries
    memcpy(gen_cache + 0, (uint8_t[]){0xF8, 0xFF, 0xFF, 0xFF}, 4);

    printf("\nFAT to be written:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    sd_write_sectors(_locate_n_FAT(0), 1, gen_cache);
    sd_write_sectors(_locate_n_FAT(1), 1, gen_cache);

    //=====Root Dir
    // zero entire Root Dir
    memset(gen_cache, 0x00, SD_BLOCK_SIZE);
    for (uint32_t i = RootDirRegionStart; i < DataRegionStart; i++) {
        sd_write_sectors(i, 1, gen_cache);
    }

    // populate with first entry
    memcpy(gen_cache + DIR_FILENAME_OFFSET, "WEST OS SD", 11);
    gen_cache[DIR_ATTR_OFFSET] = FS_ATTR_VOL; //volume name entry

    // fill in Creation time, OS needs timekeeping
    struct tm now;
    if (rtc_is_time_valid() && rtc_get_time(&now)) {
        _timestamp_dir_entry(&now, gen_cache + DIR_CREATE_TIME_OFFSET);
    }
    
    printf("\nRoot Dir to be written:\n");
    for (uint16_t i = 0; i < 512; i) {
        printf("%02X ", gen_cache[i]);
        i++;
        if (i % 16 == 0) {
            printf("\n");
        }
    }

    sd_write_sectors(RootDirRegionStart, 1, gen_cache);

    // force this since we clobbered everything, we can try to mount again
    sd_mounted = false;

    if (!fat_mount_sd(false)) {
        debug_deposit("FT16-CRIT-Failed to mount SD after formatting", 0, DBG_NULL_VAR);
        return false;
    }

    return fat_init_os_sd();
}

static void _lba_to_chs(uint32_t lba, uint8_t chs[3]) {
    enum { HEADS = 255, SPT = 63 };

    uint32_t c = lba / (HEADS * SPT);
    uint32_t tmp = lba % (HEADS * SPT);
    uint32_t h = tmp / SPT;
    uint32_t s = (tmp % SPT) + 1; // sectors are 1..63

    if (c > 1023 || h > 254 || s > 63) {
        // Not representable in MBR CHS -> use "maximum" CHS (conventional)
        chs[0] = 254;                  // head
        chs[1] = (uint8_t)(63 | 0xC0); // sector=63, cyl high bits=3 (0xC0)
        chs[2] = 0xFF;                 // cyl low 8 bits (1023 -> 0x3FF)
        return;
    }

    chs[0] = (uint8_t)h;
    chs[1] = (uint8_t)((s & 0x3F) | ((c >> 2) & 0xC0)); // s in bits 0-5, cyl[9:8] in bits 6-7
    chs[2] = (uint8_t)(c & 0xFF);                       // cyl low 8 bits
}

static void _mbr_partition_entry(uint32_t start_lba, uint32_t lba_count, uint8_t *entry16 ) {
    uint8_t chs_start[3], chs_end[3];

    // Defensive: empty partition if count == 0
    if (lba_count == 0) {
        memset(entry16, 0, 16);
        return;
    }

    uint32_t end_lba = start_lba + (lba_count - 1);

    _lba_to_chs(start_lba, chs_start);
    _lba_to_chs(end_lba,   chs_end);

    entry16[0] = 0x00;        // non-bootable
    entry16[1] = chs_start[0];
    entry16[2] = chs_start[1];
    entry16[3] = chs_start[2];
    entry16[4] = 0x0E;        // system ID (FAT16 LBA)
    entry16[5] = chs_end[0];
    entry16[6] = chs_end[1];
    entry16[7] = chs_end[2];
    w_le32(entry16, 8,  start_lba);
    w_le32(entry16, 12, lba_count);
}

static void _timestamp_dir_entry(const struct tm *now, uint8_t *bytes) {

    uint16_t fat_time = (uint16_t)(now->tm_hour << 11 |
                        now->tm_min << 5 |
                        now->tm_sec / 2);
    
                        // years since 1900 -> 1980
    uint16_t fat_date = (uint16_t)((now->tm_year - 80) << 9 |
                        (now->tm_mon + 1) << 5 |
                        (now->tm_mday));

    bytes[0] = (uint8_t)(fat_time & 0xFF);
    bytes[1] = (uint8_t)(fat_time >> 8);
    bytes[2] = (uint8_t)(fat_date & 0xFF);
    bytes[3] = (uint8_t)(fat_date >> 8);
}

void fat_timestamp_tm(const uint8_t *bytes, struct tm *now) {

    memset(now, 0, sizeof(struct tm));

    uint16_t fat_time = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    uint16_t fat_date = (uint16_t)bytes[2] | ((uint16_t)bytes[3] << 8);

    now->tm_hour = (fat_time >> 11) & 0x1F;
    now->tm_min = (fat_time >> 5) & 0x3F;
    now->tm_sec = (fat_time & 0x1F) * 2;

    now->tm_year = ((fat_date >> 9) & 0x7F) + 80; // years since 1980 -> 1900
    now->tm_mon = ((fat_date >> 5) & 0x0F) - 1;
    now->tm_mday = (fat_date & 0x1F);
}

bool fat_timestamp_bytes(fat_file_handle_t *handle, file_timestamp_t option, uint8_t *ts_bytes) {
    uint32_t dir_sector = handle->dir_entry / BytesPerSector;
    uint16_t cache_addr = handle->dir_entry % BytesPerSector;

    //  TIMESTAMP_LAST_WRITE = 1, TIMESTAMP_CREATION = 0
    uint8_t dir_offset = option ? DIR_WRITE_TIME_OFFSET : DIR_CREATE_TIME_OFFSET;
    memcpy(ts_bytes, &_load_dir_cache(dir_sector)[cache_addr + dir_offset], 4);
    if (!dir_cache_valid) return false;

    // we do not validate bytes are nonzero. Caller can do that
    return true;
}

/*************************************************
 *  fat_unmount_sd() and its helpers
*************************************************/

bool fat_unmount_sd(void) {

    if (!sd_mounted) {
        debug_deposit("FT16-CRIT-Cannot unmount as SD is marked as 'unmounted'", 0, DBG_NULL_VAR);
        return false;
    }

    if (!fat_flush()) {
        debug_deposit("FT16-WARN-Failed to flush FAT16 cache", 0, DBG_NULL_VAR);
        return false;
    }

    if (!sd_block_deinit()) {
        debug_deposit("FT16-WARN-SD Card deinit failed", 0, DBG_NULL_VAR);
        return false;
    }

    sd_mounted = false;
    
    _fat_wipe_statics();

    return true;
}

static void _fat_wipe_statics(void) {

    VolumeStart = 0;
    BytesPerSector = 0;
    SectorsPerCluster = 0;
    BytesPerCluster = 0;
    SectorsPerFAT = 0;
    TotalSectors = 0;
    NumOfFATs = 0;
    NumRootEntries = 0;
    NumFATEntries = 0;

    ReservedRegionStart = 0;
    ReservedRegionStart = 0;
    FATRegionStart = 0;
    FATRegionSize = 0;
    RootDirRegionStart = 0;
    RootDirRegionSize = 0;
    DataRegionStart = 0;
    DataRegionSize = 0;
    IndicesPerSector = 0;
}

/*************************************************
 *  fat_open() and its helpers
*************************************************/

// MUST RECEIVE AN ABSOLUTE AND NUL-TERMINATED PATH
fat_file_handle_t fat_open(const char *path, file_open_mode_t mode) {
    
    // split path into components on '/'
    char entry_name[11];
    uint16_t path_index = 0;
    bool file_found = false;
    uint32_t dir_sector = RootDirRegionStart;
    fat_file_handle_t item_handle = {0};

    // validate path a little
    if (path[path_index] != '/') {
        debug_deposit("FT16-CRIT-Path missing first '/'", 0, DBG_NULL_VAR);
        item_handle.flags = FAT_HANDLE_FLAG_INVALID;
        return item_handle;
    }

    //loop through path items
    while(!file_found) { 
        memset(entry_name, ' ', 11); //fills item name with 11 spaces

        if (path[path_index] == '\0') {
            debug_deposit("FT16-WARN-File not found. Hit '\\0' of entered path", 0, DBG_NULL_VAR);
            item_handle.flags = FAT_HANDLE_FLAG_INVALID;
            return item_handle;
        }
        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(path, path_index, entry_name);
        printf("Entry name to find: %.*s\n", 11, entry_name);

        // attempt to find entry in current directory
        if (_find_dir_entry(&item_handle, dir_sector, entry_name) < 0) {
            item_handle.flags = FAT_HANDLE_FLAG_INVALID;
            return item_handle;
        }

        // did NOT find entry
        if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) { 

            // if caller allows "CREATE" will try to create
            // and check if in final item of path
            //  \0 means _get_next_entry_name ended at \0, so this is the last item in path
            if ((mode & FS_MODE_CREATE) && (path[path_index] == '\0')) { 
                if(_create_file(entry_name, &item_handle)) {
                    printf("File not found, but created at: %08lX\n", item_handle.dir_entry);
                    file_found = true;
                    break;
                } else {
                    // volume is full
                    item_handle.flags |= FAT_HANDLE_FLAG_VOL_FULL;
                    return item_handle;
                }
            } else {
                return item_handle;
            }
            
        }

        // if the found item is not a directory, it is the target file. :)
        if (!_is_entry_directory(item_handle.attributes)) {
            printf("File found. Marking file_found = true\n");
            file_found = true;
            break;
        }
        printf("Found item is directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(item_handle.start_cluster);
    }

    // if ((item_handle.attributes & FS_ATTR_RO) && (!_fat_read_only_mode(mode))) {

    //     debug_deposit("FT16-CRIT-Tried fat_open() with non-read only mode on a Read Only file", 0, DBG_NULL_VAR);
    //     item_handle.flags = FAT_HANDLE_FLAG_INVALID;
    // }

    // do some different actions based on MODE
    item_handle.flags |= mode;
    printf("Handle Flags: 0x%02X\n", item_handle.flags);

    return item_handle;
}

static bool _fat_read_only_mode(file_open_mode_t mode) {
    if ((mode & FS_MODE_APPEND) || (mode & FS_MODE_CREATE) || (mode & FS_MODE_WRITE)) {
        return false;
    }
    if (mode & FS_MODE_READ_ONLY) {
        return true;
    }
    return false;
}

static int32_t _find_dir_entry(fat_file_handle_t *handle, uint32_t dir_sector, const char *entry_name) {
    char cur_entry[11];
    uint16_t dir_entry_index = 0;
    bool item_found = false;
    uint8_t return_flag = 0x00;
    uint32_t dir_addr_0xE5 = 0;
    printf("Searching for: %.*s\n", 11, entry_name);
    printf("...in sector: %u\n", dir_sector);
    
    while(dir_entry_index < NumRootEntries) { // all dirs will have RootDirMaxEntries limitation
        printf("Analyzing directory entry #%u\n", dir_entry_index);

        // load entry name into cur_entry
        memcpy(cur_entry, &_load_dir_cache(dir_sector)[(dir_entry_index % IndicesPerSector) * DIR_ENTRY_SIZE], 11);

        if (!dir_cache_valid) {
            printf("SEE MEEEEEEEEEEEEE!!!!\n");
            return -1;
        }

        // if entry is empty, we've hit end of directory entries. create file? fat_open() will decide
        if (cur_entry[0] == 0x00) {
            break;
        }

        //save location of first 0xE5, to use if item never found
        if ((cur_entry[0] == 0xE5) && (dir_addr_0xE5 == 0)) { 
            dir_addr_0xE5 = (dir_sector * BytesPerSector) + ((dir_entry_index % IndicesPerSector) * DIR_ENTRY_SIZE);
        }


        printf("Current Entry: %.*s\n", 11, cur_entry);

        // compare and see if you found the item
        if (memcmp(cur_entry, entry_name, 11) == 0) {
            printf("Found item!\n");
            item_found = true;
            break;
        }

        // every 16 entries, we have to go to the next sector
        //  this is nontrivial when not in root, thus _next_sect_of_item()
        if ((dir_entry_index + 1) % (IndicesPerSector) == 0) { 

            uint16_t backup_sector = dir_sector; // used if we need to claim a new cluster

            printf("Jumping to next sector:\n");
            // does ++, unless at cluster border, then goes to next cluster of dir
            dir_sector = _next_sect_of_item(dir_sector); 
            if (dir_sector < 2) { // _next_sect_of_item found EOF cluster (0xFFFF) or out of range error
                
                // checks if we hit 0xFFFF (EOC) and we have not hit max NumRootEntries for dir
                //      essentially only for claiming cluster & getting dir_entry_addr to create
                //      new file in new cluster of current directory
                if (dir_sector == 1 && ((dir_entry_index + IndicesPerSector) <= NumRootEntries)) {
                    printf("Hit last cluster of dir, but not NumRootEntries. Claiming avail cluster:\n");
                    // Claim next cluster for this directory
                    uint16_t next_cluster = _claim_avail_cluster(_cluster_given_sector(backup_sector));

                    if (next_cluster == 0) { //claiming cluster failed because volume is full
                        debug_deposit("FT16-CRIT-Item not found in dir, but dir cannot grow. Volume full", 0, DBG_NULL_VAR);
                        handle->flags = FAT_HANDLE_FLAG_VOL_FULL;
                        return 0;
                    }

                    // Empty that newly claimed cluster of this directory
                    printf("Writing 0x00's to cluster. %u writes needed\n", BytesPerCluster / SD_BLOCK_SIZE);
                    uint32_t chunk_of_cluster = 0;
                    uint32_t dir_entry_addr = _sector_given_cluster(next_cluster) * BytesPerSector;

                    while(chunk_of_cluster < (BytesPerCluster / SD_BLOCK_SIZE)) {

                        printf("Writing chunk #%u\n", chunk_of_cluster);
                        uint32_t dir_sect_addr = (dir_entry_addr + (chunk_of_cluster * SD_BLOCK_SIZE)) / BytesPerSector;
                        uint32_t dir_offset_addr = (dir_entry_addr + (chunk_of_cluster * SD_BLOCK_SIZE)) % BytesPerSector;
                        memset(_write_dir_cache(dir_sect_addr) + dir_offset_addr, 0x00, SD_BLOCK_SIZE);

                        chunk_of_cluster += 1;
                    }

                    dir_sector = _sector_given_cluster(next_cluster);
                    printf("Next cluster: %u. Next sector: %u\n", next_cluster, dir_sector);
                } else {
                    debug_deposit("FT16-WARN-Hit 0xFFFF or out of range when finding next sect. cur sect:", dir_sector, DBG_U32_HEX);
                    break;
                }                
            }
        }

        // increment dir_entry_index
        dir_entry_index += 1;
    }

    if ((dir_entry_index >= NumRootEntries) && (dir_addr_0xE5 == 0)) { // directory is full
        // will be passed to _create_file() and will fail it
        handle->flags = FAT_HANDLE_FLAG_VOL_FULL;
        debug_deposit("FT16-CRIT-_find_dir_entry() found dir full, _create_file() will fail too", 0, DBG_NULL_VAR);
        return 0;
    }

    uint32_t dir_entry_addr = (dir_sector * BytesPerSector) + ((dir_entry_index % IndicesPerSector) * DIR_ENTRY_SIZE);
    printf("Dir Entry Address: 0x%08lX", dir_entry_addr);

    // load values anyway. set flag if entry not found.
    //  this way we pass dir_entry_addr to _create_file() if 
    //  missing file should trigger creation
    _load_file_handle(dir_entry_addr, handle);

    if(!item_found) { // will tell caller, "we never found item"
        handle->flags = FAT_HANDLE_FLAG_INVALID;
        if (dir_addr_0xE5 != 0) { // we found a deleted entry, will pass to caller
            handle->dir_entry = dir_addr_0xE5;
        }
    }

    //unecessary as callers just pull dir_entry from *handle
    return dir_entry_addr;
}

static bool _load_file_handle(uint32_t dir_entry_addr, fat_file_handle_t *handle) {

    uint32_t dir_sector = dir_entry_addr / BytesPerSector;
    uint16_t cache_addr = dir_entry_addr % BytesPerSector;

    handle->attributes = _load_dir_cache(dir_sector)[cache_addr + DIR_ATTR_OFFSET];

    if (!dir_cache_valid) return false;
    
    memcpy(handle->file_name, &_load_dir_cache(dir_sector)[cache_addr + DIR_FILENAME_OFFSET], 11);
    handle->start_cluster = _le16p(&_load_dir_cache(dir_sector)[cache_addr + DIR_START_CLUSTER_OFFSET]);
    handle->file_size = _le32p(&_load_dir_cache(dir_sector)[cache_addr + DIR_FILE_SIZE_OFFSET]);
    handle->dir_entry = dir_entry_addr;

    // Removing because empty files can have 0x0000 and are not an issue
    //      we need them detected by fat_open() and fat_listdir()
    // if (handle->start_cluster == 0x0000) { //something weird and bad happened, fail
    //                                             // LFN got through somehow?
    //     handle->flags = FAT_HANDLE_FLAG_INVALID;
    // } else {
    //     handle->flags = FAT_HANDLE_FLAG_VALID; // marks handle as valid
    // }
    handle->flags = FAT_HANDLE_FLAG_VALID;

    printf("\nFile Handle details:\n");
    printf("Attributes = 0x%02X\n", handle->attributes);
    printf("File Name = %.*s\n", 11, handle->file_name);
    printf("Start Cluster = 0x%04X\n", handle->start_cluster);
    printf("File Size = %u\n", handle->file_size);
    printf("Dir Entry = 0x%08lX\n", handle->dir_entry);

    return true;
}

static uint8_t* _load_dir_cache(uint32_t lba) {
    if (dir_cache_lba == lba) return dir_cache;

    dir_cache_valid = false;

    if (!sd_mounted) {
        debug_deposit("FT16-CRIT-Attempted to read/write dir but sd_mounted = false", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (ReservedRegionSize == 0) {
        debug_deposit("FT16-CRIT-Attempted to read/write dir before drive was mounted & initialized", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (dir_cache_dirty) {
        if(!sd_write_sectors(dir_cache_lba, 1, dir_cache)) {
            debug_deposit("FT16-CRIT-Writing dirty dir cache failed. REBOOT REQUIRED. LBA: ", lba, DBG_U32_HEX);
            // this is a crazy bug/place to be
            //  we can have a cache that is dirty and needs to be written, but the write fails
            //  so we can never read a new cache. we're stuck. reboot is necessary. eek
            return dir_cache;
        }
    }

    if (!sd_read_sectors(lba, 1, dir_cache)) {
        debug_deposit("FT16-CRIT-Loading directory cache failed. LBA: ", lba, DBG_U32_HEX);
        return dir_cache;
    }

    dir_cache_dirty = false;
    dir_cache_lba = lba;
    dir_cache_valid = true;
    return dir_cache;
}

static uint32_t _next_sect_of_item(uint32_t sector) {
    printf("===============\n");
    printf("cur sector: 0x%08lX\n", sector);
    if (sector >= DataRegionStart) { // non root directory
        printf("not in root dir\n");

        uint32_t sector_in_cluster = (sector - DataRegionStart) % SectorsPerCluster;
        printf("Sector in cluster: %u\n", sector_in_cluster);

        if (sector_in_cluster == (SectorsPerCluster - 1)) { //we are on cluster border
            // the next cluster must be found in FAT
            printf("at cluster border, must find next cluster, then sector\n");

            uint16_t cluster = _cluster_given_sector(sector);
            printf("cluster: 0x%04lX\n", cluster);
            
            // finds address of cluster in FAT sector
            uint16_t cluster_offset = (cluster * 2) % BytesPerSector;
            printf("cluster offset: 0x%04X\n", cluster_offset);
            // find sector of FAT with current cluster and load into fat_cache
            //  use _le16p to get next cluster

            cluster = _le16p( &_load_fat_cache(_FAT_sect_of_cluster(0, cluster))[cluster_offset]);
            printf("Next Cluster: 0x%04lX\n", cluster);

            // check if cluster is EOF (0xFFFF). 
            //  This means we never found the next item in _find_dir_entry()
            if (cluster > 0xFFF7) { // last cluster of chain
                return 0x01;
            }
            if (cluster == 0xFFF7) { // bad cluster, ERROR
                debug_deposit("FT16-CRIT-_next_sect_of_item() returned bad cluster=0xFFF7: ", cluster, DBG_U32_HEX);
                return 0x00;
            }

            // get first sector of that cluster
            sector = _sector_given_cluster(cluster);
            printf("Sector from cluster: 0x%08lX\n", sector);

            // if you try to read out of FAT volume range, return 0x00
            if (sector > (VolumeStart + DataRegionSize - 1)) {
                debug_deposit("FT16-CRIT-_next_sect_of_item() tried to read out of volume", 0, DBG_NULL_VAR);
                return 0x00;
            }

            return sector;
        }
    }

    sector += 1;

    // if you try to read out of FAT volume range, return 0x00
    if (sector > (VolumeStart + DataRegionSize - 1)) {
        return 0x00;
    }

    // if conditions above not met, just increment sector
    return sector;
}

static uint8_t* _load_fat_cache(uint32_t lba) {
    if (fat_cache_lba == lba) return fat_cache;

    fat_cache_valid = false;

    if (!sd_mounted) {
        debug_deposit("FT16-CRIT-Attempted to read/write fat but sd_mounted = false", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (ReservedRegionSize == 0) {
        debug_deposit("FT16-CRIT-Attempted to read/write fat before drive was mounted & initialized", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (fat_cache_dirty) {
        if (!sd_write_sectors(fat_cache_lba, 1, fat_cache)) {
            debug_deposit("FT16-CRIT-Writing dirty gen cache failed. REBOOT REQUIRED. LBA: ", lba, DBG_U32_HEX);
            // this is a crazy bug/place to be
            //  we can have a cache that is dirty and needs to be written, but the write fails
            //  so we can never read a new cache. we're stuck. reboot is necessary. eek
            return fat_cache;
        }
    }

    if (!sd_read_sectors(lba, 1, fat_cache)) {
        debug_deposit("FT16-CRIT-Loading FAT cache failed. LBA: ", lba, DBG_U32_HEX);
        return fat_cache;
    }

    fat_cache_dirty = false;
    fat_cache_lba = lba;
    fat_cache_valid = true;
    return fat_cache;
}


static uint16_t _get_next_entry_name(const char *path, uint32_t path_index, uint8_t *entry_name) {

    uint8_t entry_name_index = 0;
    uint32_t path_length = strlen(path);

    // not really that safe, depends on nul-terminated path
    while (path_index < path_length) {
        path_index++;

        if (path[path_index] == '.') { // 
            entry_name_index = 8;
            continue;

        } else if (path[path_index] == '/' || path[path_index] == '\0') {
            break;

        } else {
            entry_name[entry_name_index] = path[path_index];
            entry_name_index += 1;
        }
    }

    return path_index;
}

static bool _create_file(const char *file_name, fat_file_handle_t *handle) {

    // check if _find_dir_entry() found directory full (passed as FAT_HANDLE_FLAG_VOL_FULL)
    if (handle->flags & FAT_HANDLE_FLAG_VOL_FULL) {
        printf("Too many dir entries (512). Will not create file\n");
        return false;        
    }

    //create 32B entry
    uint8_t dir_entry_bytes[DIR_ENTRY_SIZE] = {0x00};

    //fill it with: name, attribute, first cluster
    memcpy(dir_entry_bytes, file_name, 11);
    dir_entry_bytes[DIR_ATTR_OFFSET] = 0x20; // setting attributes

    // new files get start cluster of 0x0000 until first fat_write()
    uint16_t avail_cluster = 0x0000;
    
    w_le16(dir_entry_bytes, DIR_START_CLUSTER_OFFSET, avail_cluster);

    // add creation time for timekeeping
    struct tm now;
    if (rtc_is_time_valid() && rtc_get_time(&now)) {
        _timestamp_dir_entry(&now, dir_entry_bytes + DIR_CREATE_TIME_OFFSET);
        _timestamp_dir_entry(&now, dir_entry_bytes + DIR_WRITE_TIME_OFFSET);
    }

    printf("Writing this directory entry:\n");
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");

    //write it to cache/SD
    uint32_t dir_sect_addr = handle->dir_entry / BytesPerSector;
    uint32_t dir_offset_addr = handle->dir_entry % BytesPerSector;
    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    //empty and fill handle
    memcpy(handle->file_name, file_name, 11);
    handle->attributes = dir_entry_bytes[DIR_ATTR_OFFSET];
    handle->start_cluster = avail_cluster;
    handle->file_size = 0;
    handle->flags = FAT_HANDLE_FLAG_VALID;

    return true;
}

static uint16_t _claim_avail_cluster(uint16_t last_cluster) {
    uint16_t read_cluster = 0xFFFF;
    uint16_t cluster_index = 2;
    uint32_t cluster_sect_addr = 0;

    // find next available cluster
    while (read_cluster != 0x0000 && cluster_index < NumFATEntries) {

        cluster_sect_addr = (cluster_index * 2) % BytesPerSector;
        read_cluster = 
        _le16p( &_load_fat_cache(_FAT_sect_of_cluster(0, cluster_index))[cluster_sect_addr]);

        printf("Cluster contents: 0x%04lX\n", read_cluster);

        cluster_index += 1;
    }

    if (read_cluster) {
        // disk full, return error code
        debug_deposit("FT16-CRIT-Volume is full. Could not find free cluster", 0, DBG_NULL_VAR);
        return 0;
    }

    cluster_index -= 1;
    cluster_sect_addr = (cluster_index * 2) % BytesPerSector; //redundant, but helps readability

    printf("Empty cluster to use: 0x%04lX\n", cluster_index);
    
    // mark cluster in FATs (all 2 FATs)
    for (uint8_t i = 0; i < NumOfFATs; i++) {
        _write_fat_cache(_FAT_sect_of_cluster(i, cluster_index))[cluster_sect_addr] = 0xFF;
        _write_fat_cache(_FAT_sect_of_cluster(i, cluster_index))[cluster_sect_addr+1] = 0xFF;
    }
    
    // if prev_clust != 0, mark that cluster from 0xFFFF to cluster_index
    printf("writing prev cluster (%u) to point to new cluster (%u)\n", last_cluster, cluster_index);
    if (last_cluster) {
        cluster_sect_addr = (last_cluster * 2) % BytesPerSector;
        for (uint8_t i = 0; i < NumOfFATs; i++) {
            _write_fat_cache(_FAT_sect_of_cluster(i, last_cluster))[cluster_sect_addr] = cluster_index & 0xFF;
            _write_fat_cache(_FAT_sect_of_cluster(i, last_cluster))[cluster_sect_addr+1] = cluster_index >> 8;
        }
        
    }
    return cluster_index;
}


/*************************************************
 *  fat_read() and its helpers
*************************************************/

// returns number of read bytes
int32_t fat_read(fat_file_handle_t *handle, uint32_t offset, uint8_t *dst, uint32_t len) {

    // validity checks
    if (offset > handle->file_size || len > handle->file_size - offset) { // overflow errors
        debug_deposit("FT16-CRIT-Read failed. offset + len caused overflow. offset:", offset, DBG_U32_DEC);
        return -1;
    }
    if (!handle || (!dst && len)) { // NULL variables
        debug_deposit("FT16-CRIT-Read failed. handle or dst == NULL", 0, DBG_NULL_VAR);
        return -1; 
    }
    if (len == 0) return 0;

    // 1. Convert offest into sector
    //  must march through sectors & clusters
    printf("File's first cluster: 0x%08lX\n", handle->start_cluster);
    uint32_t sector = _sector_given_cluster(handle->start_cluster);
    printf("File's first sector: 0x%04lX\n", sector);

    uint32_t cluster_jumps = offset / (BytesPerSector * SectorsPerCluster);
    while (cluster_jumps > 0) {
        sector += SectorsPerCluster - 1;
        sector = _next_sect_of_item(sector);
        if (sector < 2) {
            debug_deposit("FT16-CRIT-Failed to find next sector in fat_read(). 0xFFFF or out of range", 0, DBG_NULL_VAR);
            return -1;
        }
        cluster_jumps -= 1;
        offset -= (BytesPerSector * SectorsPerCluster);
    }

    sector += offset / BytesPerSector;
    // now offset is between 0 and 511
    offset = offset % BytesPerSector;

    // 2. Read off sector (sd_read_sector)
    //  may have to pad/shift data for offset sector
    //  may need to read across clusters
    uint32_t len_written = 0;
    while (len > 0) {
        uint32_t left_in_sector = BytesPerSector - offset;
        uint32_t len_to_write = (len < left_in_sector) ? len : left_in_sector;

        memcpy(dst + len_written, _load_gen_cache(sector) + offset, len_to_write);

        if (!gen_cache_valid) return -1;

        if (!gen_cache_valid) {
            debug_deposit("FT16-CRIT-Failed to read data. LBA: ", sector, DBG_U32_HEX);
            return -1;
        }

        offset = 0;
        len_written += len_to_write;
        len -= len_to_write;

        if (len > 0) {
            sector = _next_sect_of_item(sector);
            if (sector < 2) {
                debug_deposit("FT16-CRIT-Failed to find next sector while reading. 0xFFFF or out of range", 0, DBG_NULL_VAR);
                return -1;
            }
        }
        
    }

    // 3. Return length read
    return len_written;
}

static uint8_t* _load_gen_cache(uint32_t lba) {
    if (gen_cache_lba == lba) return gen_cache;

    gen_cache_valid = false;

    if (!sd_mounted) {
        debug_deposit("FT16-CRIT-Attempted to read/write gen but sd_mounted = false", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (ReservedRegionSize == 0) {
        debug_deposit("FT16-CRIT-Attempted to read/write gen before drive was mounted & initialized", 0, DBG_NULL_VAR);
        return NULL;
    }

    if (gen_cache_dirty) {
        if(!sd_write_sectors(gen_cache_lba, 1, gen_cache)) {
            debug_deposit("FT16-CRIT-Writing dirty gen cache failed. REBOOT REQUIRED. LBA: ", lba, DBG_U32_HEX);
            // this is a crazy bug/place to be
            //  we can have a cache that is dirty and needs to be written, but the write fails
            //  so we can never read a new cache. we're stuck. reboot is necessary. eek
            return gen_cache;
        }
    }
    
    if (!sd_read_sectors(lba, 1, gen_cache)) {
        debug_deposit("FT16-CRIT-Loading gen cache failed. LBA: ", lba, DBG_U32_HEX);
        return gen_cache;
    }
    
    gen_cache_dirty = false;
    gen_cache_lba = lba;
    gen_cache_valid = true;
    return gen_cache;
}



/*************************************************
 *  fat_write() and its helpers
*************************************************/

int32_t fat_write(fat_file_handle_t *handle, uint32_t offset, const uint8_t *src, uint32_t len) {

    uint32_t original_offset = offset;

    // validity checks
    if (offset > handle->file_size) { // overflow errors
        debug_deposit("FT16-CRIT-Write failed. offset caused overflow. offset:", offset, DBG_U32_DEC);
        return -1;
    }
    if (!handle || (!src && len)) { // NULL variables
        debug_deposit("FT16-CRIT-Write failed. handle or dst == NULL", 0, DBG_NULL_VAR);
        return -1; 
    }
    if ((handle->flags & FS_MODE_READ_ONLY) || (handle->attributes & FS_ATTR_RO)) {
        debug_deposit("FT16-CRIT-Tried to write to READ_ONLY file handle", handle->flags, DBG_U8_HEX);
        return -1;
    }
    if (len == 0) return 0;
    printf("Writing FAT passed validity checks\n");

    // 0. If start_cluster = 0x0000, we need to allocate it:
    if (handle->start_cluster == 0x0000) {
        handle->start_cluster = _claim_avail_cluster(0);
    }

    // 1. Convert offest into sector
    //  must march through sectors & clusters
    uint32_t sector = _sector_given_cluster(handle->start_cluster);

    uint32_t cluster_jumps = offset / (BytesPerSector * SectorsPerCluster);
    printf("Cluster Jumps needed: %u\n", cluster_jumps);
    while (cluster_jumps > 0) {
        sector += SectorsPerCluster - 1;

        cluster_jumps -= 1;
        offset -= (BytesPerSector * SectorsPerCluster);

        uint32_t temp_sector = _next_sect_of_item(sector);

        if (temp_sector < 2) {//error, 0 = out of volume, 1 = last clust is 0xFFFF (CLAIM NEW ONE)
            if (temp_sector == 1) { // we need to claim a new sector

                printf("claiming new sector in cluster jump\n");
                uint16_t temp_clust = _cluster_given_sector(sector);
                printf("prev clust: %u\n", temp_clust);
                temp_clust = _claim_avail_cluster(temp_clust);
                printf("claimed clust: %u\n", temp_clust);

                if (temp_clust == 0) { // could not claim cluster, because volume is full
                    debug_deposit("FT16-CRIT-fat_write() failed. Tried to claim cluster to write, but volume full", 0, DBG_NULL_VAR);
                }

                printf("Cluster claimed: %04lX\n", temp_clust);
                sector = _sector_given_cluster(temp_clust);
            } else {
                debug_deposit("FT16-CRIT-Failed to find next sector in fat_write(). 0xFFF7 - bad cluster", 0, DBG_NULL_VAR);
                return -1;
            }
        } else {
            sector = temp_sector;
        }

        
    }

    sector += offset / BytesPerSector;
    // now offset is between 0 and 511
    offset = offset % BytesPerSector;

    // 2. Write to sector
    //  may have to pad/shift data for offset sector
    //  may need to write across clusters
    int32_t len_written = 0;
    while (len > 0) {
        uint32_t left_in_sector = BytesPerSector - offset;
        uint32_t len_to_write = (len < left_in_sector) ? len : left_in_sector;
        printf("Writing %u bytes...\n", len_to_write);

        memcpy(_write_gen_cache(sector) + offset, src + len_written, len_to_write);

        if (!gen_cache_valid) return -1;

        if (!gen_cache_valid) {
            debug_deposit("FT16-CRIT-Failed to write data. LBA: ", sector, DBG_U32_HEX);
            return -1;
        }

        uint16_t cluster = _cluster_given_sector(sector); // stores cluster in case
        //  we need to claim a new cluster below, we need to know the current one

        offset = 0;
        len_written += len_to_write;
        len -= len_to_write;

        if (len > 0) {
            sector = _next_sect_of_item(sector);
            if (sector < 2) { //error, 0 = out of volume, 1 = last clust is 0xFFFF (CLAIM NEW ONE)
                if (sector == 1) { // we need to claim a new cluster to write in
                    printf("Last cluster was 0xFFFF. Claiming new cluster:\n");
                    cluster = _claim_avail_cluster(cluster);

                    if (cluster == 0) { // could not claim cluster, because volume is full
                        debug_deposit("FT16-CRIT-fat_write() failed. Tried to claim cluster to write, but volume full", 0, DBG_NULL_VAR);
                    }

                    printf("Cluster: %04lX\n", cluster);
                    sector = _sector_given_cluster(cluster);
                }
            }
        }

        
    }
    uint32_t write_end = original_offset + (uint32_t) len_written;
    if (write_end > handle->file_size) {
        handle->file_size = write_end;
    }

    return len_written;
}

static uint8_t* _write_gen_cache(uint32_t lba) {

    if (ReservedRegionSize == 0 || sd_mounted == false) {
        debug_deposit("FT16-CRIT-Attempted to write gen cache, but sd_mounted or ResvRegSize invalid", 0, DBG_NULL_VAR);
        gen_cache_valid = false;
    }

    if (lba == gen_cache_lba) {
        // do nothing
    } else {
        _load_gen_cache(lba);
    }

    gen_cache_dirty = true;
    return gen_cache;
}

static uint8_t* _write_fat_cache(uint32_t lba) {

    if (ReservedRegionSize == 0 || sd_mounted == false) {
        debug_deposit("FT16-CRIT-Attempted to write fat cache, but sd_mounted or ResvRegSize invalid", 0, DBG_NULL_VAR);
        fat_cache_valid = false;
    }

    if (lba == fat_cache_lba) {
        // do nothing
    } else {
        _load_fat_cache(lba);
    }

    fat_cache_dirty = true;
    return fat_cache;
}

static uint8_t* _write_dir_cache(uint32_t lba) {

    if (ReservedRegionSize == 0 || sd_mounted == false) {
        debug_deposit("FT16-CRIT-Attempted to write dir cache, but sd_mounted or ResvRegSize invalid", 0, DBG_NULL_VAR);
        dir_cache_valid = false;
    }
    
    if (lba == dir_cache_lba) {
        // do nothing
    } else {
        _load_dir_cache(lba);
    }

    dir_cache_dirty = true;
    return dir_cache;
}

/*************************************************
 *  fat_flush() and its helpers
*************************************************/

bool fat_flush(void) {
    if (gen_cache_dirty) {
        sd_write_sectors(gen_cache_lba, 1, gen_cache);
        gen_cache_dirty = false;
    }
    if (fat_cache_dirty) {
        sd_write_sectors(fat_cache_lba, 1, fat_cache);
        fat_cache_dirty = false;
    }
    if (dir_cache_dirty) {
        sd_write_sectors(dir_cache_lba, 1, dir_cache);
        dir_cache_dirty = false;
    }
    return true;
}

/*************************************************
 *  fat_move() and its helpers
*************************************************/

bool fat_move(const char *src_path, const char *dst_path) {

    //=== Get fat_file_handle of src_path ===
    fat_file_handle_t old_file = fat_open(src_path, FS_MODE_READ_ONLY);

    if ((old_file.flags & FAT_HANDLE_FLAG_VALID) == 0x00) {
        debug_deposit("FT16-CRIT-fat_move() could not fat_open() src file. Invalid flag", 0, DBG_NULL_VAR);
        return false;
    }

    if ((old_file.attributes & FS_ATTR_RO)) {
        debug_deposit("FT16-CRIT-fat_move() tried moving Read Only file", 0, DBG_NULL_VAR);
        return false;
    }

    printf("Finished opening file/getting file handle. File name: %.*s\n", 11, old_file.file_name);

    // pull dir_entry_bytes from src_path
    uint8_t dir_entry_bytes[DIR_ENTRY_SIZE] = {0};
    uint32_t dir_sect_addr = old_file.dir_entry / BytesPerSector;
    uint32_t dir_offset_addr = old_file.dir_entry % BytesPerSector;

    // fill dir_entry_bytes with all the data (will all stay the same except file name)
    memcpy(dir_entry_bytes, _load_dir_cache(dir_sect_addr) + dir_offset_addr, DIR_ENTRY_SIZE);

    if (!dir_cache_valid) return false;

    

    printf("Source directory entry:\n");
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");

    //=== Get dst_path directory sector/offset & file_name
    fat_file_handle_t new_file = {0};
    uint16_t path_index = 0;
    uint32_t dir_sector = RootDirRegionStart;
    bool file_found = false;

    // validate path a little:
    if (dst_path[path_index] != '/') {
        debug_deposit("FT16-CRIT-Destination file path missing first '/'", 0, DBG_NULL_VAR);
        new_file.flags = FAT_HANDLE_FLAG_INVALID;
        return false;
    }

    //loop through path items
    while(!file_found) { 
        memset(new_file.file_name, ' ', 11); //fills item name with 11 spaces

        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(dst_path, path_index, new_file.file_name);
        printf("Entry name to find: %.*s\n", 11, new_file.file_name);

        // LOAD NEW FILE NAME into dir_entry_bytes because _find_dir_entry() will wipe file_name
        //      this will happen multiple times, but the last is all we care about
        memcpy(dir_entry_bytes, new_file.file_name, 11);

        // attempt to find entry in current directory
        _find_dir_entry(&new_file, dir_sector, new_file.file_name);

        // did NOT find entry
        if ((new_file.flags & FAT_HANDLE_FLAG_VALID) == 0x00) {  //file not found at dir, good

            // check if in final item of path
            //  \0 means _get_next_entry_name ended at \0, so this is the last item in path
            if ((dst_path[path_index] == '\0')) { 
                if(_create_file(new_file.file_name, &new_file)) {
                    printf("File not found, created at: %08lX\n", new_file.dir_entry);
                    file_found = true;
                    break;
                } else {
                    // volume is full
                    debug_deposit("FT16-CRIT-Could not move file because volume is full", 0, DBG_NULL_VAR);
                    new_file.flags |= FAT_HANDLE_FLAG_VOL_FULL; // no one will see this...
                    return false;
                }
            } else { // we just did not find a folder on path
                break;
            }
            
        }

        // if the found item is not a directory, it is the target file. :)
        if (!_is_entry_directory(new_file.attributes)) {
            printf("File found. Marking file_found = true\n");
            file_found = true;

            // if the file exists and was not created, we must truncate it
            fat_truncxtend(&new_file, 0);

            break;
        }
        printf("Found item is directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(new_file.start_cluster);
    }

    // check output of while loop:
    if (!file_found) { //some directory in dst_path is invalid
        debug_deposit("FT16-WARN-fat_move() failed due to missing directory in dst_path", 0, DBG_NULL_VAR);
        return false;
    }
    printf("Destination file found or created\n");

    // mark old file dir entry as deleted, this might be redundant, but it's fine
    //      these dir_sect_addr & dir_offset_addr were defined way up there ^^
    _write_dir_cache(dir_sect_addr)[dir_offset_addr] = DIR_DELETED_ENTRY;

    printf("Writing this directory entry:\n");
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");

    //=== Write fat_file_handle.dir_entry[32] to directory sector/offset
    dir_sect_addr = new_file.dir_entry / BytesPerSector;
    dir_offset_addr = new_file.dir_entry % BytesPerSector;
    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    return fat_flush();
 }

/*************************************************
 *  fat_copy() and its helpers
*************************************************/

bool fat_copy(const char *src_path, const char *dst_path) {

    // fat_open(src)
    fat_file_handle_t src_handle = fat_open(src_path, FS_MODE_READ_ONLY);
    
    // fat_open(dst) 
    fat_file_handle_t dst_handle = fat_open(dst_path, FS_MODE_WRITE | FS_MODE_CREATE);

    if ((src_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) {
        debug_deposit("FAT16-CRIT-fat_copy() could not fat_open() src file. Invalid flag", 0, DBG_NULL_VAR);
        return false;
    }
    if ((dst_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) {
        debug_deposit("FAT16-CRIT-fat_copy() could not fat_open() dst file. Invalid flag", 0, DBG_NULL_VAR);
        return false;
    }

    // must zero out destination file
    dst_handle.file_size = 0;
    
    // loop: fat_read() & fat_write()
    uint32_t bytes_written = 0;
    bool transfer_failed = false;

    while ((bytes_written + FAT_SPARE_BUF_SIZE) <= src_handle.file_size) {
        printf("Reading for copying:\n");
        if (fat_read(&src_handle, bytes_written, spare_buf, FAT_SPARE_BUF_SIZE) < 0) {
            transfer_failed = true;
            break;
        }

        printf("Writing for copying:\n");
        int32_t write_return = fat_write(&dst_handle, bytes_written, spare_buf, FAT_SPARE_BUF_SIZE);
        if (write_return < 0) {
            transfer_failed = true;
            break;
        }
        
        bytes_written += (uint32_t) write_return;
        
        printf("Writing bytes for copy. %u bytes written so far...\n", bytes_written);
    }

    if (transfer_failed) {
        debug_deposit("FT16-WARN-File copy failed due to fat_read() or fat_write() failure", 0, DBG_NULL_VAR);
        return false;
    }
    
    //transfer the remaning >FAT_COPY_BUF_SIZE bytes
    if (fat_read(&src_handle, bytes_written, spare_buf, src_handle.file_size - bytes_written) < 0) {
        debug_deposit("FT16-WARN-File copy failed due to final fat_read() failure", 0, DBG_NULL_VAR);
        return false;
    }
    if (fat_write(&dst_handle, bytes_written, spare_buf, src_handle.file_size - bytes_written) < 0) {
        debug_deposit("FT16-WARN-File copy failed due to final fat_write() failure", 0, DBG_NULL_VAR);
        return false;
    }
    

    // fat_close(both)
    // fat_close(&src_handle); // removed because copy was not modified in any way
    fat_close(&dst_handle);

    return true;
}

/*************************************************
 *  fat_close() and its helpers
*************************************************/

bool fat_close(fat_file_handle_t *handle) {
    uint8_t dir_entry_bytes[DIR_ENTRY_SIZE] = {0x00};
    uint32_t dir_sect_addr = handle->dir_entry / BytesPerSector;
    uint32_t dir_offset_addr = handle->dir_entry % BytesPerSector;

    memcpy(dir_entry_bytes, _load_dir_cache(dir_sect_addr) + dir_offset_addr, DIR_ENTRY_SIZE);

    if (!dir_cache_valid) return false;

    memcpy(dir_entry_bytes, handle->file_name, 11);
    dir_entry_bytes[DIR_ATTR_OFFSET] = handle->attributes;
    w_le16(dir_entry_bytes, DIR_START_CLUSTER_OFFSET, handle->start_cluster);
    w_le32(dir_entry_bytes, DIR_FILE_SIZE_OFFSET, handle->file_size);

    // add/update 'last modified' time
    struct tm now;
    // gen_cache_dirty will only occur if changes to data were made (not dir, not fat)
    //      I'm pretty sure this check might cause timestamp bugs, but it's small potatoes 
    if (gen_cache_dirty && rtc_is_time_valid() && rtc_get_time(&now)) {
        _timestamp_dir_entry(&now, dir_entry_bytes + DIR_WRITE_TIME_OFFSET);
    }

    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    fat_flush();

    return true;
}

/*************************************************
 *  fat_truncxtend() and its helpers
*************************************************/

bool fat_truncxtend(fat_file_handle_t *handle, uint32_t new_size) {

    if (handle->attributes & FS_ATTR_RO) {
        debug_deposit("FT16-CRIT-Tried to fat_truncxtend() file with Read Only attribute set", 0, DBG_NULL_VAR);
        return false;
    }

    if (new_size <= handle->file_size) { // shrinking the file
        // "<=" because we want to delete folders with this too, which always have 0 "file size"

        // Calculate # of clusters to deallocate
        uint16_t old_cluster_count =  (handle->file_size + BytesPerCluster - 1) / BytesPerCluster;
        uint16_t new_cluster_count = (new_size + BytesPerCluster - 1) / BytesPerCluster;
        uint16_t clusters_to_chop = old_cluster_count - new_cluster_count;

        printf("Old Size: %u. New size: %u\n", handle->file_size, new_size);
        printf("Clusters to chop: %u\n", clusters_to_chop);

        //=== loop # from above
        while (clusters_to_chop) {

            // find last & penultimate cluster
            uint16_t read_cluster = handle->start_cluster;
            uint16_t ult_cluster = 0x0000; //ultimate (last)
            uint16_t pen_cluster = 0x0000; //penultimate (2nd to last)
            uint16_t cluster_sect_addr = 0;

            while (read_cluster < 0xFFF7) {
                pen_cluster = ult_cluster;
                ult_cluster = read_cluster;

                cluster_sect_addr = (read_cluster * 2) % BytesPerSector;
                read_cluster = _le16p( &_load_fat_cache(_FAT_sect_of_cluster(0, read_cluster))[cluster_sect_addr]);

                if (!fat_cache_valid) return false;
            }

            // mark last as 0x0000 in all FATs
            printf("Marking ult cluster %u as 0x0000\n", ult_cluster);
            cluster_sect_addr = (ult_cluster * 2) % BytesPerSector;
            for (uint8_t i = 0; i < NumOfFATs; i++) {
                w_le16(_write_fat_cache(_FAT_sect_of_cluster(i, ult_cluster)), cluster_sect_addr, 0x0000);
            }
            
            // mark penultimate as 0xFFFF
            if (pen_cluster) {
                printf("Marking pen cluster %u as 0xFFFF\n", pen_cluster);
                cluster_sect_addr = (pen_cluster * 2) % BytesPerSector;
                for (uint8_t i = 0; i < NumOfFATs; i++) {
                    w_le16(_write_fat_cache(_FAT_sect_of_cluster(i, pen_cluster)), cluster_sect_addr, 0xFFFF);
                }
            }

            clusters_to_chop -= 1;
        }


    } else { // growing file
        // WON'T REALLY USE THIS (for now)
    }

    handle->file_size = new_size;
    if (new_size == 0) handle->start_cluster = 0x0000;
    
    return fat_close(handle);
}

/*************************************************
 *  fat_mkdir() and its helpers
*************************************************/

bool fat_mkdir(const char *path) {

    // move path into spare_buf because we need to modify it
    memcpy(spare_buf, path, strlen(path) + 1);

    if (!_trim_terminal_slash(spare_buf)) {
        debug_deposit("FT16-CRIT-fat_opendir() path is null or 0 len. length: ", 0, DBG_NULL_VAR);
        return false;
    }

    // split path into components on '/'
    char entry_name[11];
    uint16_t path_index = 0;
    bool folder_found = false;
    uint32_t dir_sector = RootDirRegionStart;
    fat_file_handle_t item_handle = {0};
    uint16_t parent_dir_cluster = 0x0000;

    // validate path a little
    if (spare_buf[path_index] != '/') {
        debug_deposit("FT16-CRIT-fat_mkdir(): Path missing first '/' or trying to make root dir?", 0, DBG_NULL_VAR);
        return false;
    }

    //loop through path items
    while(!folder_found) { 
        memset(entry_name, ' ', 11); //fills item name with 11 spaces

        if (spare_buf[path_index] == '\0') {
            debug_deposit("FT16-WARN-mkdir folder already exists? Hit '\\0' of entered path", 0, DBG_NULL_VAR);
            item_handle.flags = FAT_HANDLE_FLAG_INVALID;
            return false;
        }
        //^pointless, but don't want to break things
        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(spare_buf, path_index, entry_name);
        printf("Entry name to find: %.*s\n", 11, entry_name);

        // attempt to find entry in current directory
        if (_find_dir_entry(&item_handle, dir_sector, entry_name) < 0) return false;

        // did NOT find entry
        if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) { 
            //^pointless, but don't want to break things

            // check if in final item of path
            //  \0 means _get_next_entry_name ended at \0, so this is the last item in path
            if (spare_buf[path_index] == '\0') { 
                if(_create_dir(entry_name, &item_handle)) {
                    printf("Folder not found, but created at: %08lX\n", item_handle.dir_entry);
                    folder_found = true;
                    break;
                } else {
                    // volume is full
                    debug_deposit("FT16-WARN-Could not mkdir. Name too long or volume full", 0, DBG_NULL_VAR);
                    return false;
                }
            } else { // did not an find item in path before last item
                debug_deposit("FT16-WARN-mkdir() could not find path entered to make dir", 0, DBG_NULL_VAR);
                return false;
            }
            
        }

        printf("Found directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(item_handle.start_cluster);
    }

    fat_flush(); // ensures all the dir_cache writes get written to disk

    // TODO: printf some details about item_handle (the new folder)
    return true;
}

static bool _create_dir(const char *dir_name, fat_file_handle_t *handle) {

    // with handle, we have: dir_entry
    uint32_t dir_entry_addr = handle->dir_entry;

    if (dir_name[8] != 0x20 || dir_name[9] != 0x20 || dir_name[10] != 0x20) {
        // folder name too long, uses EXT section of FILENAME. NOT ALLOWED.
        // TODO: technically directories can have EXTensions. FIX someday
        debug_deposit("FT16-CRIT-_create_folder() failed, folder name too long", 0, DBG_NULL_VAR);
        return false;
    }

    uint8_t dir_entry_bytes[DIR_ENTRY_SIZE] = {0x00};

    //===fill and write directory buffer
    uint16_t avail_cluster = _claim_avail_cluster(0);
    if (avail_cluster == 0x0000) {
        // volume is full
        debug_deposit("FT16-CRIT-Volume is full, cannot claim cluster for _create_folder()", 0, DBG_NULL_VAR);
        return false;
    }
    w_le16(dir_entry_bytes, DIR_START_CLUSTER_OFFSET, avail_cluster);

    memcpy(dir_entry_bytes, dir_name, 11);
    dir_entry_bytes[DIR_ATTR_OFFSET] = FS_ATTR_DIR;

    // add creation time for timekeeping
    struct tm now;
    if (rtc_is_time_valid() && rtc_get_time(&now)) {
        _timestamp_dir_entry(&now, dir_entry_bytes + DIR_CREATE_TIME_OFFSET);
        _timestamp_dir_entry(&now, dir_entry_bytes + DIR_WRITE_TIME_OFFSET);
    }

    printf("Writing this directory entry in parent dir:\n");
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");
    
    uint32_t dir_sect_addr = dir_entry_addr / BytesPerSector;
    uint32_t dir_offset_addr = dir_entry_addr % BytesPerSector;
    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    //===go to directory's cluster and fill with . and .. entries
    // technically returns sector, but beginning of sector is where first dir entry goes
    dir_entry_addr = _sector_given_cluster(avail_cluster) * BytesPerSector; 

    // First fill new dir cluster with 0x00's
    uint32_t chunk_of_cluster = 0;
    // memset(spare_buf, 0x00, FAT_SPARE_BUF_SIZE);

    printf("Writing 0x00's to cluster. %u writes needed\n", BytesPerCluster / SD_BLOCK_SIZE);
    while(chunk_of_cluster < (BytesPerCluster / SD_BLOCK_SIZE)) {

        printf("Writing chunk #%u\n", chunk_of_cluster);
        dir_sect_addr = (dir_entry_addr + (chunk_of_cluster * SD_BLOCK_SIZE)) / BytesPerSector;
        dir_offset_addr = (dir_entry_addr + (chunk_of_cluster * SD_BLOCK_SIZE)) % BytesPerSector;
        memset(_write_dir_cache(dir_sect_addr) + dir_offset_addr, 0x00, SD_BLOCK_SIZE);

        chunk_of_cluster += 1;
    }

    // . directory entry fill and write
    memset(dir_entry_bytes, 0x20, 11);
    dir_entry_bytes[0] = '.';

    printf("Writing the '.' entry at 0x%u [B]:\n", dir_entry_addr);
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");

    dir_sect_addr = dir_entry_addr / BytesPerSector;
    dir_offset_addr = dir_entry_addr % BytesPerSector;
    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    // .. directory entry fill and write
    uint16_t parent_cluster = 0x0000; // default root dir cluster
    if ((handle->dir_entry / BytesPerSector) >= DataRegionStart) { // parent dir is NOT root dir
        parent_cluster = _cluster_given_sector(handle->dir_entry / BytesPerSector);
    }

    dir_entry_bytes[1] = '.'; // the rest of the dir name is already properly formatted
    w_le16(dir_entry_bytes, DIR_START_CLUSTER_OFFSET, parent_cluster);

    dir_entry_addr += DIR_ENTRY_SIZE;

    printf("Writing the '..' entry at 0x%u [B]:\n", dir_entry_addr);
    for (uint8_t i = 0; i < DIR_ENTRY_SIZE; i++) {
        printf("0x%02X ", dir_entry_bytes[i]);
    }
    printf("\n");

    dir_sect_addr = dir_entry_addr / BytesPerSector;
    dir_offset_addr = dir_entry_addr % BytesPerSector;
    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr, dir_entry_bytes, DIR_ENTRY_SIZE);

    return true;
}

/*************************************************
 *  fat_delete() and its helpers
*************************************************/

bool fat_delete(const char *path) {
    fat_file_handle_t handle = fat_open(path, FS_MODE_WRITE);

    if ((handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) {
        debug_deposit("FT16-CRIT-fat_delete() could not find file", 0, DBG_NULL_VAR);
        return false;
    }

    if (handle.attributes & FS_ATTR_RO) {
        debug_deposit("FT16-CRIT-fat_delete() tried to delete Read Only file", 0, DBG_NULL_VAR);
        return false;
    }

    //=== deallocate all clusters
    if (!fat_truncxtend(&handle, 0)) { // will write new size and start cluster to dir entry on SD and handle
        debug_deposit("FT16-CRIT-fat_delete() failed at truncxtend", 0, DBG_NULL_VAR);
        return false;
    }

    //=== mark entry as deleted (0xE5), see next command
    handle.file_name[0] = DIR_DELETED_ENTRY;

    // fat_close() will write file_name (with 0xE5) to directory
    return fat_close(&handle);
}

/*************************************************
 *  fat_rmdir() and its helpers
*************************************************/

bool fat_rmdir(const char *path) {

    // move path into spare_buf because we need to modify it
    memcpy(spare_buf, path, strlen(path) + 1);

    if (!_trim_terminal_slash(spare_buf)) {
        debug_deposit("FT16-CRIT-fat_rmdir() path is null or 0 len. length: ", 0, DBG_NULL_VAR);
        return false;
    }

    if (strlen(spare_buf) == 0) {
        debug_deposit("FT16-CRIT-fat_rmdir() attempted to remove root dir '/'", 0, DBG_NULL_VAR);
        return false;
    }

    if (fat_opendir(path, true, NULL) > 0) { // check if dir is empty
        debug_deposit("FT16-CRIT-fat_rmdir() tried to remove non-empty directory", 0, DBG_NULL_VAR);
        return false;
    }

    // split path into components on '/'
    char entry_name[11];
    uint16_t path_index = 0;
    bool folder_found = false;
    uint32_t dir_sector = RootDirRegionStart;
    fat_file_handle_t item_handle = {0};
    uint16_t parent_dir_cluster = 0x0000;

    // validate path a little
    if (spare_buf[path_index] != '/') {
        debug_deposit("FT16-CRIT-fat_rmdir(): Path missing first '/'", 0, DBG_NULL_VAR);
        return false;
    }

    //loop through path items
    while(!folder_found) { 
        memset(entry_name, ' ', 11); //fills item name with 11 spaces

        if (spare_buf[path_index] == '\0') {
            debug_deposit("FT16-WARN-File not found. Hit '\\0' of entered path", 0, DBG_NULL_VAR);
            item_handle.flags = FAT_HANDLE_FLAG_INVALID;
            return false;
        }
        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(spare_buf, path_index, entry_name);
        printf("Entry name to find: %.*s\n", 11, entry_name);

        // attempt to find entry in current directory
        if (_find_dir_entry(&item_handle, dir_sector, entry_name) < 0) return false;

        // did NOT find entry
        if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) { 

            debug_deposit("FT16-WARN-rmdir()) could not find path entered to remove dir", 0, DBG_NULL_VAR);
            return false;
        }

        if (spare_buf[path_index] == '\0') {
            printf("Found folder %.*s!\n", 11, item_handle.file_name);
            folder_found = true; // a little redundant, but I don't want the printf below firing
            break;
        }

        printf("Found directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(item_handle.start_cluster);
    }

    // check a few things: is a folder and file_size
    if ((item_handle.attributes & FS_ATTR_DIR) == 0x00 || item_handle.file_size > 0) {
        debug_deposit("FT16-CRIT-fat_rmdir() pulled non dir handle. showed >0 size or not dir attr", 0, DBG_NULL_VAR);
        return false;
    }

    if (item_handle.attributes & FS_ATTR_RO) {
        debug_deposit("FT16-CRIT-Tried to fat_rmdir() a Read Only directory", 0, DBG_NULL_VAR);
        return false;
    }

    // now item_handle has 
    //      dir_entry = LBA & offset of directory in parent directory
    //      file_size = 0
    //      start_cluster = start cluster of directory

    //=== deallocate cluster chain
    uint16_t cluster = item_handle.start_cluster;
    uint16_t temp_cluster;
    uint32_t cluster_sect_addr;
    // keep walking until 0xFFFF
    while (cluster < 0xFFF8) {

        cluster_sect_addr = (cluster * 2) % BytesPerSector;
        printf("Marking dir cluster %u as 0x0000\n", cluster);
        // storing because after for loop, FAT entry is 0x0000
        temp_cluster = _le16p( &_load_fat_cache(_FAT_sect_of_cluster(0, cluster))[cluster_sect_addr]);

        for (uint8_t i = 0; i < NumOfFATs; i++) {
            w_le16(_write_fat_cache(_FAT_sect_of_cluster(i, cluster)), cluster_sect_addr, 0x0000);
        }

        cluster = temp_cluster;
        // serious errors in chain:
        if (cluster < 2 || cluster == 0xFFF7) {
            debug_deposit("FT16-CRIT-fat_rmdir() found corrupt cluster chain or 0xFFF7 cluster: ", cluster, DBG_U32_HEX);
        }
    }

    //=== mark entry as deleted (0xE5)
    item_handle.file_name[0] = DIR_DELETED_ENTRY;
    // fat_close will write 0xE5 to dir entry

    return fat_close(&item_handle);
}

/*************************************************
 *  fat_opendir() & fat_listdir() and their helpers
*************************************************/

// opens a directory and saves off it's starting cluster as static
//  this function works in junction with fat_listdir()
int32_t fat_opendir(const char *path, bool real_files_only, fat_dir_list_t *dir) {

    uint16_t list_dir_sector;

    // move path into spare_buf because we need to modify it
    memcpy(spare_buf, path, strlen(path) + 1);

    if (!_trim_terminal_slash(spare_buf)) {
        debug_deposit("FT16-CRIT-fat_opendir() path is null or 0 len. length: ", 0, DBG_NULL_VAR);
        return -1;
    }

    // split path into components on '/'
    char entry_name[11];
    uint16_t path_index = 0;
    bool folder_found = false;
    uint32_t dir_sector = RootDirRegionStart;
    fat_file_handle_t item_handle = {0};
    bool in_root = false;

    printf("path: ");
    printf(spare_buf);
    printf("\n");

    if (strlen(spare_buf) == 0) { //we are in root directory
        printf("Asked to opendir() a root directory\n");
        in_root = true;
        folder_found = true;
        item_handle.attributes = FS_ATTR_DIR; // not accurate for root, but needed for logic below
        item_handle.file_size = 0;
        // these bypass the logic below and jump down to where we need to be
    }

    // validate path a little
    if (spare_buf[path_index] != '/' && !in_root) {
        debug_deposit("FT16-CRIT-opendir(): Path missing first '/'", 0, DBG_NULL_VAR);
        return -1;
    }

    //loop through path items
    while(!folder_found) { 
        memset(entry_name, ' ', 11); //fills item name with 11 spaces

        if (spare_buf[path_index] == '\0') {
            debug_deposit("FT16-WARN-File not found. Hit '\\0' of entered path", 0, DBG_NULL_VAR);
            item_handle.flags = FAT_HANDLE_FLAG_INVALID;
            return -1;
        }
        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(spare_buf, path_index, entry_name);
        printf("Entry name to find: %.*s\n", 11, entry_name);

        // attempt to find entry in current directory
        if (_find_dir_entry(&item_handle, dir_sector, entry_name) < 0) return -1;

        // did NOT find entry
        if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0x00) { 

            debug_deposit("FT16-WARN-opendir() could not find path entered to open dir", 0, DBG_NULL_VAR);
            return -1;
        }

        if (spare_buf[path_index] == '\0') {
            printf("Found folder %.*s!\n", 11, item_handle.file_name);
            folder_found = true; // a little redundant, but I don't want the printf below firing
            break;
        }

        printf("Found directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(item_handle.start_cluster);
    }

    // check a few things: is a folder and file_size
    if ((item_handle.attributes & FS_ATTR_DIR) == 0x00 || item_handle.file_size > 0) {
        debug_deposit("FT16-CRIT-opendir() pulled non dir handle. showed >0 size or not dir attr", 0, DBG_NULL_VAR);
        return -1;
    }

    // set important static variable, used in fat_listdir() iterated
    if (in_root) {
        list_dir_sector = RootDirRegionStart;
    } else {
        list_dir_sector = _sector_given_cluster(item_handle.start_cluster);
    }
    
    uint8_t dir_first_byte = 0xFF;
    uint8_t dir_attr_byte = 0x00;
    int32_t num_entries = 0; // count all directories (.,..,0xE5). fat_listdir() can filter
    uint32_t entry_index = 0;
    uint16_t sector = list_dir_sector;
    printf("list_dir_sector = 0x%08lX\n", list_dir_sector);

    while (dir_first_byte != 0x00) {

        dir_first_byte = _load_dir_cache(sector)[(entry_index % IndicesPerSector) * DIR_ENTRY_SIZE];
        dir_attr_byte = 
            _load_dir_cache(sector)[((entry_index % IndicesPerSector) * DIR_ENTRY_SIZE) + DIR_ATTR_OFFSET];
        printf("dir first byte = 0x%02X\n", dir_first_byte);

        if (!dir_cache_valid) return -1;

        // if we are only counting real files, we don't count deleted (0xE5) entries
        if (real_files_only && !_entry_is_not_deleted(dir_first_byte, dir_attr_byte)) { 
            // do nothing
        } else {
            num_entries += 1;
        }
        entry_index += 1;

        if (entry_index == IndicesPerSector) {
            sector = _next_sect_of_item(sector);
            printf("Must jump sectors. Next sect: %08lX\n", sector);
            
            if (sector == 1) break; //hit end of cluster chain, no more entries
            if (sector == 0) {
                debug_deposit("FT16-CRIT-Bad cluster (0xFFF7) or read out of volume", 0, DBG_NULL_VAR);
                return -1;
                break;
            }

        }
    }

    if (real_files_only) { // if we only count real files, we have to remove some entries:
        if (in_root) {
            num_entries -= 1; // remove volume name entry
        } else {
            num_entries -= 2; // remove 2 dir (.,..) entries
        }
    }

    if (dir) {
        dir->num_entries = ((uint16_t)num_entries > 0) ? ((uint16_t)num_entries - 1) : 0;
        dir->list_dir_sector = list_dir_sector;
        dir->index = 0;
    }
    
    return (num_entries > 0) ? (num_entries - 1) : 0; // because we count the first 0x00 we hit
}

bool fat_listdir(fat_dir_list_t *dir, fat_file_handle_t *handle) {

    if (dir->index >= dir->num_entries) { // tried to read from an index in dir that is out of bounds
        return false;
    }

    uint16_t index = dir->index % IndicesPerSector;
    printf("looking at index #%u in sector: %08lX\n", index, dir->list_dir_sector);

    uint32_t dir_entry_addr = (dir->list_dir_sector * BytesPerSector) + (index * DIR_ENTRY_SIZE);

    if (!_load_file_handle(dir_entry_addr, handle)) return false;

    if (handle->file_name[0] == DIR_DELETED_ENTRY) {
        printf("found deleted entry\n");
        // deleted entry, don't call "_load_file_handle() on it"
        //      but still return it
        handle->flags = FAT_HANDLE_FLAG_INVALID;
    } else {
        
        printf("found real entry. Name: %.*s\n", 11, handle->file_name);
    }

    if ((index + 1) == IndicesPerSector) {
        dir->list_dir_sector = _next_sect_of_item(dir->list_dir_sector);
        // no checks for bad (0xFFF7) or EOC (0xFFFF) next sector
        //      fat_opendir() & fat_listdir() must be called in rapid succession
        //      if there was any cluster issue, it would show up in fat_opendir() return (-1)
        //      and end of cluster is captured by caller incrementing and tracking "index"
    }

    // dir->index += 1;
    return true;
}

static bool _entry_is_not_deleted(uint8_t dir_first_byte, uint8_t dir_attr_byte) {
    if (dir_first_byte != 0xE5) {
        return true;
    } else {
        return false;
    }
}

// when a path is for a directory, users sometimes terminate with a '/'
//  this filters it out.
static bool _trim_terminal_slash(char *path) {
    if (!path) return false;

    uint16_t path_len = strlen(path);
    if (path_len == 0) return false;

    printf("path length = %u\n", path_len);
    printf("path before trim: ");
    printf(path);
    printf("\n");

    if (path[path_len - 1] == '/') {
        path[path_len - 1] = '\0';
    }

    printf("path after trim: ");
    printf(path);
    printf("\n");

    return true;
}


/*************************************************
 *  fat_cluster_usage() and its helpers
*************************************************/

uint8_t fat_cluster_usage(void) {
    uint16_t claimed_clusters = 0;
    uint32_t cluster_sect_addr = 0;

    // NumFATEntries != last entry, == num of entries
    for (uint32_t i = 2; i <= NumFATEntries + 1; i++) {
        
        cluster_sect_addr = (i * 2) % BytesPerSector;
        uint16_t read_cluster = _le16p( &_load_fat_cache(_FAT_sect_of_cluster(0, i))[cluster_sect_addr]);

        // we consider everything 'claimed' (bad, EOC, etc)
        if (read_cluster != 0x0000) {
            claimed_clusters += 1;
        }
    }

    if (!fat_cache_valid) return 0;

    // computes percentage of claiming and returns 0-100
    uint8_t usage = (uint8_t) ((uint32_t)claimed_clusters * 100 / NumFATEntries);
    return (claimed_clusters > 0 && usage == 0) ? 1 : usage;
}

/*************************************************
 *  fat_init_os_sd() and its helpers
*************************************************/

bool fat_init_os_sd(void) {

    fat_mkdir("/DOCS");
    fat_mkdir("/LOGS");

    fat_file_handle_t info_handle = fat_open("/INFO.TXT", FS_MODE_WRITE | FS_MODE_CREATE);
    fat_write(&info_handle, 0, info_doc_string, INFO_DOC_LEN);
    fat_close(&info_handle);
}


/*************************************************
 *  Entry attribute modifiers
*************************************************/

bool fat_find_entry(const char * path, fat_file_handle_t *item_handle) {

    char entry_name[11];
    uint16_t path_index = 0;
    bool item_found = false;
    uint32_t dir_sector = RootDirRegionStart;

    if (!path) {
        return false;
    }

    // move path into spare_buf because we need to modify it
    memcpy(spare_buf, path, strlen(path) + 1);

    // trims terminal slash if it is there
    if (!_trim_terminal_slash(spare_buf)) {
        debug_deposit("FT16-CRIT-fat_find_entry() path is null or 0 len. length: ", 0, DBG_NULL_VAR);
        return false;
    }

    // validate path a little
    if (spare_buf[path_index] != '/') {
        debug_deposit("FT16-CRIT-fat_find_entry(): Path missing first '/'", 0, DBG_NULL_VAR);
        return false;
    }

    //loop through path items
    while(!item_found) { 
        memset(entry_name, ' ', 11); //fills item name with 11 spaces

        if (spare_buf[path_index] == '\0') {
            debug_deposit("FT16-WARN-File not found. Hit '\\0' of entered path", 0, DBG_NULL_VAR);
            return false;
        }
        printf("Path Index before 'next entry name': %u\n", path_index);

        path_index = _get_next_entry_name(spare_buf, path_index, entry_name);
        printf("Entry name to find: %.*s\n", 11, entry_name);

        // attempt to find entry in current directory
        if (_find_dir_entry(item_handle, dir_sector, entry_name) < 0) {
            return false;
        }

        // did NOT find entry
        if ((item_handle->flags & FAT_HANDLE_FLAG_VALID) == 0x00) {
            return false;
        }

        if (spare_buf[path_index] == '\0') {
            printf("Found entry %.*s!\n", 11, item_handle->file_name);
            item_found = true; // a little redundant, but I don't want the printf below firing
            break;
        }

        printf("Found directory, going a level deeper:\n\n\n");

        dir_sector = _sector_given_cluster(item_handle->start_cluster);
    }

    return item_found;
}

bool fat_get_attr(const char *path, uint8_t *attr) {
    printf("Getting entry attr byte\n");

    if (!attr) return false;

    fat_file_handle_t item_handle = {0};
    if (!fat_find_entry(path, &item_handle)) return false;

    if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        return false;
    }

    *attr = item_handle.attributes;
    
    return true;
}


bool fat_set_attr(const char *path, uint8_t attr) {

    printf("attr byte: %02X\n", attr);

    fat_file_handle_t item_handle = {0};
    if (!fat_find_entry(path, &item_handle)) return false;

    if ((item_handle.flags & FAT_HANDLE_FLAG_VALID) == 0) {
        return false;
    }

    uint32_t dir_entry_addr = item_handle.dir_entry;
    uint32_t dir_sect_addr = dir_entry_addr / BytesPerSector;
    uint32_t dir_offset_addr = dir_entry_addr % BytesPerSector;

    printf("writing attr byte: %02X\n", attr);

    memcpy(_write_dir_cache(dir_sect_addr) + dir_offset_addr + DIR_ATTR_OFFSET, &attr, 1);
    if (!dir_cache_valid) return false;

    // For directories, we also have to change the '.' entry in the dir itself
    if (attr & FS_ATTR_DIR) {

        uint32_t sect = _sector_given_cluster(item_handle.start_cluster);
        memcpy(_write_dir_cache(sect) + DIR_ATTR_OFFSET, &attr, 1);
        if (!dir_cache_valid) return false;
    }

    return fat_flush();
}

/*************************************************
 *  Encryption Needs
*************************************************/

bool fat_read_16B(uint32_t addr, uint8_t *buf) {
    if (addr & 0x0F) return false; // verifies addr is 16-byte aligned
        // necessary for _load_gen_cache()

    uint32_t sector = addr / BytesPerSector;
    uint32_t offset = addr % BytesPerSector;

    memcpy(buf, _load_gen_cache(sector) + offset, 16);

    return gen_cache_valid;
}

bool fat_write_16B(uint32_t addr, uint8_t *buf) {
    if (addr & 0x0F) return false; // verifies addr is 16-byte aligned
        // necessary for _load_gen_cache()

    uint32_t sector = addr / BytesPerSector;
    uint32_t offset = addr % BytesPerSector;

    memcpy(_write_gen_cache(sector) + offset, buf, 16);

    return gen_cache_valid;
}

uint32_t fat_addr_given_cluster(uint16_t cluster) {
    return _sector_given_cluster(cluster) * BytesPerSector;
}

uint16_t fat_next_cluster(uint16_t prev_cluster) {
    uint32_t cluster_sect_addr = (prev_cluster * 2) % BytesPerSector;
    return _le16p(&_load_fat_cache(_FAT_sect_of_cluster(0, prev_cluster))[cluster_sect_addr]);
}

uint32_t fat_get_bytes_per_cluster(void) {
    return BytesPerCluster;
}

















// KNOWN BUGS
//  - we do not truncxtend clusters of a directory, so after a dir claims a cluster it is always used
//          files can get truncxtended by VFS because they have a file size
//          directory clusters CAN get deallocated if the directory is rmdir()





static uint32_t _locate_n_FAT(uint8_t fat_copy) {
    return FATRegionStart + (fat_copy * SectorsPerFAT);
}

static uint16_t _num_FAT_entries(void) {
    return (DataRegionSize + SectorsPerCluster - 1) / SectorsPerCluster;
}

static uint32_t _FAT_sect_of_cluster(uint8_t fat_copy, uint16_t cluster) {
    return _locate_n_FAT(fat_copy) + ((cluster * 2) / BytesPerSector);
}

static uint32_t _sector_given_cluster(uint16_t cluster) {
    return DataRegionStart + ((cluster - 2) * SectorsPerCluster);
}

static uint32_t _cluster_given_sector(uint32_t sector) {
    return ((sector - DataRegionStart) / SectorsPerCluster) + 2;
}


static uint32_t _le32p(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t _le32(uint32_t val) {
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8 ) | ((val & 0xFF000000) >> 24 );
}

static uint16_t _le16p(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint16_t _le16(uint16_t val) {
    return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8);
}

static void w_le16(uint8_t *buf, size_t offset, uint16_t val)
{
    buf[offset + 0] = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
}

static void w_le32(uint8_t *buf, size_t offset, uint32_t val)
{
    buf[offset + 0] = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    buf[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

static bool _is_entry_directory(uint8_t attr) {
    return ((attr & FS_ATTR_DIR) == FS_ATTR_DIR);
}