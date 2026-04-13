#include "../commands.h"
#include "uedit.h"




bool cmd_uedit_help(const out_t *out) {
    out->write(out->ctx, "Simple text editor\n uedit [-r/f] [file]\n  -r to open file in read-only, -f for forcing open System files");
    return true;
}

bool cmd_uedit_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_uedit_help(out);
}

bool cmd_uedit(uint8_t argc, const char ** argv, const out_t *out) {
    
    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_uedit_help(out);
    }

    if (argc < 2 || argc > 3) return cmd_uedit_fail(out);

    bool r_flag = false;
    bool f_flag = false;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "rf", &state)) != -1) {

        switch(c) {
            case 'r':
                r_flag = true;
                break;
            case 'f':
                f_flag = true;
                break;
            default:
                return cmd_uedit_fail(out);
        }

    }

    const char *target_file;
    if (state.optind < argc) {
        target_file = argv[state.optind];
    } else {
        return cmd_uedit_fail(out);
    }

    if (!uedit_pass_file(target_file, r_flag, f_flag, out)) {
        // uedit_pass_file() handles error message to shell
        memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);
        return true;
    } 

    prog_switch(&uedit_prog);
    return true;
}