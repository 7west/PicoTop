
#include "../commands.h"
#include "vfs.h"




bool cmd_hexdump_help(const out_t *out) {
    out->write(out->ctx, "prints hex values of a file. Probably shouldn't use if for long files");
    return true;
}

bool cmd_hexdump_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_hexdump_help(out);
}

bool cmd_hexdump(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_hexdump_help(out);
    }

    if (argc != 2) {
        return cmd_hexdump_fail(out);
    }

    char hex[6];

    file_desc_t fd = vfs_open(argv[1], FS_MODE_READ_ONLY | FS_MODE_FORCE);

    if (fd < 1) {
        out->write(out->ctx, "Invalid File");
        return true;
    }

    uint32_t file_size = vfs_get_file_size(fd);
    uint8_t symbol;

    for (uint32_t i = 0; i < file_size; i++) {

        vfs_read(fd, &symbol, 1);

        snprintf(hex, 6, "0x%02X ", symbol);
        
        if(!out->write(out->ctx, hex)) break;
    }

    vfs_close(fd);
    
    return true;
}