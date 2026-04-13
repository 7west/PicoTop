#include "../commands.h"
#include "vfs.h"
#include "fat16.h"
#include "usb_kbd.h"





// SD MOUNT COMMAND

bool cmd_sd_mount_help(const out_t *out) {
    out->write(out->ctx, "Initializes SD card and FAT16 volume on it");
    return true;
}

bool cmd_sd_mount_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_sd_mount_fail(out);
}

bool cmd_sd_mount(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_sd_mount_help(out);
    }

    if (argc != 1) {
        return cmd_sd_mount_fail(out);
    }

    if (vfs_init()) {
        out->write(out->ctx, "SD mounted successfully");
    } else {
        out->write(out->ctx, "SD failed to mount. Dump debug log");
    }
    return true;
}

// SD UNMOUNT COMMAND

bool cmd_sd_unmount_help(const out_t *out) {
    out->write(out->ctx, "Flushes caches and unmounts SD card");
    return true;
}

bool cmd_sd_unmount_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_sd_unmount_fail(out);
}

bool cmd_sd_unmount(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_sd_unmount_help(out);
    }

    if (argc != 1) {
        return cmd_sd_unmount_fail(out);
    }

    if (fat_unmount_sd()) {
        out->write(out->ctx, "SD unmounted successfully. Safe to remove");
    } else {
        out->write(out->ctx, "SD failed to unmount. Dump debug log");
    }
    return true;
}

// SD FORMAT COMMAND

bool cmd_sd_format_help(const out_t *out) {
    out->write(out->ctx, "Formats plugged in SD card to FAT16. Also writes necessary system files");
    return true;
}

bool cmd_sd_format_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_sd_format_fail(out);
}

bool cmd_sd_format(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_sd_format_help(out);
    }

    if (argc != 1) {
        return cmd_sd_format_fail(out);
    }

    out->write(out->ctx, "This will erase everything! Enter 'y' to confirm:\n");

    key_event_t key;

    while (1) { // waiting for any key
        if (!keyboard_queue_pop(&key)) {
            __wfi();
            continue;
        } else {
            break;
        }
    }

    if (key.keycode == 'y') { 
        out->write(out->ctx, "y");

        while (1) {
            if (!keyboard_queue_pop(&key)) {
                __wfi();
                continue;
            } else {
                break;
            }            
        }

    } else { // any key besides 'y' aborts
        out->write(out->ctx, "\nFormat aborted!");
        return true;
    }

    if (key.keycode == KBD_BASCII_NEW_LINE) {
        out->write(out->ctx, "\nFormat started...");

        if (fat_format_sd()) {
            out->write(out->ctx, "\nFormat successful");
        } else {
            out->write(out->ctx, "\nFormat failed. Dump debug log");
        }

    } else {
        out->write(out->ctx, "\nFormat aborted!");
        return true;
    }

    return true;
}

// SD USAGE COMMAND

bool cmd_sd_usage_help(const out_t *out) {
    out->write(out->ctx, "Mostly-accurate way of seeing %% of SD usage");
    return true;
}

bool cmd_sd_usage_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_sd_format_fail(out);
}

bool cmd_sd_usage(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_sd_usage_help(out);
    }

    if (argc != 1) {
        return cmd_sd_usage_fail(out);
    }

    uint8_t usage = fat_cluster_usage();

    out->write(out->ctx, "SD Usage: ");
    char usage_string [8];
    snprintf(usage_string, sizeof(usage_string), "%u%%", usage);
    
    out->write(out->ctx, usage_string);
    return true;
}