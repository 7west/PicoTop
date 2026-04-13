#include "../commands.h"
#include "buzz.h"





bool cmd_beep_help(const out_t *out) {
    out->write(out->ctx, "Plays one of many system beeps. Track = 1-5");
    return true;
}

bool cmd_beep_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_beep_help(out);
}

bool cmd_beep(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_beep_help(out);
    }

    if (argc != 2) {
        return cmd_beep_fail(out);
    }

    if (buzz_play_library(argv[1][0] - '0')) {
        // print nothing
    } else {
        out->write(out->ctx, "Track # not in library");
    }

    return true;
}