#include "../commands.h"
#include "ptable.h"




bool cmd_ptable_help(const out_t *out) {
    out->write(out->ctx, "Periodic Table of Elements");
    return true;
}

bool cmd_ptable_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_ptable_help(out);
}

bool cmd_ptable(uint8_t argc, const char ** argv, const out_t *out) {
    
    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_ptable_help(out);
    }

    if (argc != 1) return cmd_ptable_fail(out);

    if (argc == 1) {
        prog_switch(&ptable_prog);
        return true;
    } 
}