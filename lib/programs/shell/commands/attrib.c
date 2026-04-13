
#include "../commands.h"
#include "vfs.h"




bool cmd_attr_help(const out_t *out) {
    out->write(out->ctx, "Can read and modify the Read Only, System, and Hidden attributes of files and directories\n attr -g/-s [H/S/R] -v [1/0] [file/dir]\n  get/set Hidden/System/Read Only\n  -g takes no argument, all 3 get printed. -s needs -v");
    return true;
}

bool cmd_attr_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_attr_help(out);
}

void cmd_attr_one(const out_t *out, bool val) {
    if (val) {
        out->write(out->ctx, "1");
    } else {
        out->write(out->ctx, "0");
    }
}

uint8_t cmd_attr_change_bit(uint8_t var, uint8_t mask, bool val) {
    return val ? (var | mask) : (var & ~mask);
}

// attrib info.txt -g
// attrib info.txt -s R -v 1

bool cmd_attr(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_attr_help(out);
    }

    if (argc != 3 && argc != 6) {
        return cmd_attr_fail(out);
    }

    bool g_flag = false;
    bool s_flag = false;
    bool v_flag = false;
    const char *param = 0;
    const char *val = 0;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "gs:v:", &state)) != -1) {

        switch(c) {
            case 'g':
                g_flag = true;
                break;
            case 's':
                param = state.optarg;
                s_flag = true;
                break;
            case 'v':
                val = state.optarg;
                v_flag = true;
                break;
            default:
                return cmd_attr_fail(out);
        }
    }

    const char *target_file;
    if (state.optind < argc) {
        target_file = argv[state.optind];
    } else {
        return cmd_attr_fail(out);
    }

    // check if item exists
    if (!vfs_exists(target_file)) {
        out->write(out->ctx, "File or Directory does not exist");
        return true;
    }

    uint8_t attr = 0;
    vfs_get_attr(target_file, &attr);

    if (g_flag && (s_flag || v_flag)) cmd_attr_fail(out);
    if (s_flag ^ v_flag) return cmd_attr_fail(out); // That is XOR

    if (g_flag) {
        out->write(out->ctx, "System: ");
        cmd_attr_one(out, attr & FS_ATTR_SYS);
        out->write(out->ctx, "\nHidden: ");
        cmd_attr_one(out, attr & FS_ATTR_HID);
        out->write(out->ctx, "\nRead Only: ");
        cmd_attr_one(out, attr & FS_ATTR_RO);

        return true;
    } else if (s_flag) {

        if (*param == 'R') {
            attr = cmd_attr_change_bit(attr, FS_ATTR_RO, *val - '0');
        } else if (*param == 'S') {
            attr = cmd_attr_change_bit(attr, FS_ATTR_SYS, *val - '0');
        } else if (*param == 'H') {
            attr = cmd_attr_change_bit(attr, FS_ATTR_HID, *val - '0');
        } else {
            return cmd_attr_fail(out);
        }

        if(vfs_set_attr(target_file, attr)) {
            out->write(out->ctx, "Write successful");
        } else {
            out->write(out->ctx, "Write failed");
        }
        return true;

    } else {
        return cmd_attr_fail(out);
    }


}