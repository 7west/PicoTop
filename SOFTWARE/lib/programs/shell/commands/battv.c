#include "../commands.h"
#include "batt.h"





bool cmd_battv_help(const out_t *out) {
    out->write(out->ctx, "displays the current battery voltage. +/- 0.05V.\nFor LiPo batteries 4.2V is full, 3.2V is very low.\n The battery is around 3.7V most of the time.");
    return true;
}

bool cmd_battv_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_battv_help(out);
}

bool cmd_battv(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_battv_help(out);
    }

    if (argc != 1) {
        return cmd_battv_fail(out);
    }

    uint16_t voltage = get_batt_voltage();

    out->write(out->ctx, "Battery Voltage: ");

    char batt_string [8];
    snprintf(batt_string, sizeof(batt_string), "%u.%02u V", voltage / 100, voltage % 100);

    out->write(out->ctx, batt_string);
    return true;
}