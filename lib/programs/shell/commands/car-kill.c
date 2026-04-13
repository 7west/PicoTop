#include "../commands.h"
#include "vfs.h"
#include "os_debug.h"

#define BUF_SIZE 16

file_desc_t fd;

bool cmd_car_kill_help(const out_t *out) {
    out->write(out->ctx, "removes all Carriage Return's (0x0D's) from a file");
    return true;
}

bool cmd_car_kill_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_car_kill_help(out);
}

bool cmd_car_kill_error(const out_t *out) {
    out->write(out->ctx, "Something went wrong. Dump the debug log");
    if (fd > 0) vfs_close(fd);
    return true;
}


bool cmd_car_kill(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_car_kill_help(out);
    }

    if (argc != 2) {
        return cmd_car_kill_fail(out);
    }

    fd = vfs_open(argv[1], FS_MODE_WRITE);

    if (fd == -1) {
        out->write(out->ctx, "Invalid File");
        return true;
    } else if (fd == 0) {
        out->write(out->ctx, "File already open. Cannot edit an open file");
        return true;
    }

    out->write(out->ctx, "Removing all carriage returns... ");

    uint32_t file_size = vfs_get_file_size(fd);
    uint32_t read_offset = 0;
    uint32_t write_offset = 0;
    uint32_t cars_removed = 0;
    uint8_t buffer[BUF_SIZE] = {0};
    uint32_t temp_return = 0;

    while (read_offset < file_size) {

        uint32_t len = (file_size - read_offset > BUF_SIZE) ? BUF_SIZE : (file_size - read_offset);

        vfs_seek(fd, read_offset, FS_SEEK_START);
        temp_return = vfs_read(fd, buffer, len);
        if (temp_return != len) {
            debug_deposit("VFSL-CRIT-Carriage killer command failed at vfs_read(). len returned:", temp_return, DBG_U32_DEC);
            return cmd_car_kill_error(out);
        }

        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < len; read_idx++) {
            if (buffer[read_idx] != '\r') {
                buffer[write_idx] = buffer[read_idx];
                write_idx++;
            }
        }

        if (write_idx > 0) {
            vfs_seek(fd, write_offset, FS_SEEK_START);
            temp_return = vfs_write(fd, buffer, write_idx);
            if (temp_return != write_idx) {
                debug_deposit("VFSL-CRIT-Carriage killer command failed at vfs_write(). len returned:", temp_return, DBG_U32_DEC);
                return cmd_car_kill_error(out);
            }
        }

        read_offset += len;
        write_offset += write_idx;
        cars_removed += (len - write_idx);
    }

    if (!vfs_truncate(fd, file_size - cars_removed)) {
        debug_deposit("VFSL-CRIT-Carriage killer command failed at vfs_truncate", 0, DBG_NULL_VAR);
        return cmd_car_kill_error(out);
    }

    vfs_close(fd);

    out->write(out->ctx, "done");

    return true;
}
