#include "../commands.h"






bool cmd_pinout_help(const out_t *out) {
    out->write(out->ctx, "prints location of PicoTop pins and orientation guides");
    return true;
}

bool cmd_pinout_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_pinout_help(out);
}

bool cmd_pinout(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_pinout_help(out);
    }

    if (argc != 1) {
        return cmd_pinout_fail(out);
    }

    out->write(out->ctx, " GP9 | GND | SCL | DRX | GND | SWD\n ====+=====+=====+=====+=====+====\n GP8 | 3V3 | SDA | DTX | 3V3 | SCK\n");
    char dir_string[] =  "    HDMI                    USB   ";
    dir_string[2] = 0x1B; // left arrow
    dir_string[32] = 0x1A; // right arrow
    out->write(out->ctx, dir_string);

}