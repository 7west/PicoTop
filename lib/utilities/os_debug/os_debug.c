#include "os_debug.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "buzz.h"
#include "vfs.h"
#include "rtc.h"


static os_debug_entry_t os_debug_entries[DEBUG_MAX_ENTRIES] = {};
static uint32_t os_debug_num_entries = 0;

static file_desc_t fd_log = 0;
static uint32_t last_filed_entry = 0;

static bool vfs_failed_prev = false;


void debug_deposit(const char * msg, uint32_t var, os_debug_type_t type) {

    // efficiency trick to avoid "% DEBUG_MAX_ENTRIES"
    uint8_t index = os_debug_num_entries & (DEBUG_MAX_ENTRIES - 1);

    os_debug_entries[index].msg = msg;
    os_debug_entries[index].var = var;
    os_debug_entries[index].type = type;

    // This is kind of computationally expensive.
    //  checks if debug log type is "INFO" or not
    if (msg[5] != 'I') buzz_play_library(2);

    os_debug_num_entries += 1;

    // this code should be commented out. Only uncomment when weird bugs happen
    //  and you NEED debug output
    // char out[81];
    // debug_withdraw_entry(index, out);
    // printf("%s\n", out);

}



uint32_t debug_get_total_entries() {

    return os_debug_num_entries;
}


void debug_withdraw_entry(uint32_t abs_idx, char *out) {

    uint8_t log_idx = abs_idx & (DEBUG_MAX_ENTRIES - 1);

    os_debug_entry_t entry = os_debug_entries[log_idx];

    switch (entry.type) {
        case DBG_NULL_VAR:
            snprintf(out, 81, "%s", entry.msg);
            break;
            
        case DBG_U32_DEC:
            snprintf(out, 81, "%s %lu", entry.msg, (unsigned long)entry.var);
            break;

        case DBG_U32_HEX:
            snprintf(out, 81, "%s 0x%08lX", entry.msg, (unsigned long)entry.var);
            break;

        case DBG_U8_HEX:
            snprintf(out, 81, "%s 0x%02X", entry.msg, (uint8_t)entry.var);
            break;
    }

}

void debug_file_periodic(void) {

    // is this the first run? open the log file in append mode
    if (fd_log == 0) {
        fd_log = vfs_open("/LOGS/DEBUG.LOG", FS_MODE_APPEND | FS_MODE_CREATE);

        if (fd_log < 0) {

            if (!vfs_failed_prev) {
                debug_deposit("DBGL-WARN-Failed to open debug.log file to write debug log entries", 0, DBG_NULL_VAR);
                vfs_failed_prev = true;
            }

            fd_log = 0;
            return;
        }
    }

    // track entries written to log file, and those not yet written
    uint32_t total_entries = os_debug_num_entries;
    uint32_t unwritten_count = total_entries - last_filed_entry;
    if (unwritten_count == 0) return;

    if (unwritten_count > DEBUG_MAX_ENTRIES) {
        unwritten_count = DEBUG_MAX_ENTRIES;
        last_filed_entry = total_entries - DEBUG_MAX_ENTRIES;
    }

    // write entries to log file with accompanying timestamp (check if rtc valid)
    for (uint32_t i = 0; i < unwritten_count; i++) {

        char write_buf[81] = {0};

        // fill write_buf with timestamp
        rtc_get_time_string(write_buf);
        printf("writing %s\n", write_buf);
        memcpy(write_buf + 16, " - ", 3);

        vfs_write(fd_log, write_buf, 19);

        debug_withdraw_entry(last_filed_entry + i, write_buf);
        printf("writing %s\n", write_buf);
        uint8_t write_len = strlen(write_buf);
        write_buf[write_len] = '\n';
        
        vfs_write(fd_log, write_buf, write_len + 1);

    }

    last_filed_entry = total_entries;

    // vfs_fsync() the file
    vfs_fsync(fd_log);
    // vfs_close(fd_log);
}

void debug_print_log_UART(void) {

    uint32_t total_entries = debug_get_total_entries();

    if (total_entries == 0) return;

    uint32_t entries_to_print = total_entries;
    if (entries_to_print > DEBUG_MAX_ENTRIES) {
        entries_to_print = DEBUG_MAX_ENTRIES;
    }

    uint32_t start_index = total_entries - entries_to_print;

    char out[81];

    for (uint32_t i = 0; i < entries_to_print; i++) {
        
        debug_withdraw_entry(start_index + i, out);

        printf("%s\n", out);
    }
}