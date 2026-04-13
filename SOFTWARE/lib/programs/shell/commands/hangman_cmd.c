#include "../commands.h"
#include "hangman.h"




bool cmd_hangman_help(const out_t *out) {
    out->write(out->ctx, " ");
    return true;
}

bool cmd_hangman_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_hangman_help(out);
}

bool cmd_hangman(uint8_t argc, const char ** argv, const out_t *out) {
    
    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_hangman_help(out);
    }

    if (argc != 1) return cmd_hangman_fail(out);

    if (argc == 1) {
        prog_switch(&hangman_prog);
        return true;
    } 
}