#include "commands.h"



void getopt_init(getopt_state_t *state) {

    *state = (getopt_state_t) {
        .optarg = NULL,
        .optind = 1,
        .scanpos = NULL,
    };
}

int32_t getopt(uint8_t argc, const char ** argv, const char *optstr, getopt_state_t *state) {

    int32_t current_opt = 0;

    if (state->optind >= argc) return -1;
    if (argv[state->optind][0] != '-') return -1;
    if (argv[state->optind][1] == '\0') return -1;
    if (argv[state->optind][1] == '-') {
        state->optind += 1;
        return -1;
    }

    if (state->scanpos == NULL || *state->scanpos == '\0') {
        state->scanpos = (argv[state->optind] + 1);
    }

    current_opt = *state->scanpos;
    state->scanpos += 1;

    bool in_arg = false;
    uint8_t optstr_len = strlen(optstr);
    uint8_t idx = 0;
    while (idx < optstr_len) {

        if (optstr[idx] == current_opt) {
            in_arg = true;
            break;
        }
        idx += 1;
    }
    idx += 1;

    // option letter not in optstr
    if (!in_arg) return '?';

    if (optstr[idx] != ':') {
        if (*state->scanpos == '\0') {
            state->optind += 1;
            state->scanpos = NULL;
        }
        return current_opt;
    }

    state->optind += 1;
    // user forgot the value
    if (state->optind >= argc) return ':';

    // pass the pointer for the value
    state->optarg = argv[state->optind];

    // go look at next arg
    state->optind += 1;

    // reset scanpos
    state->scanpos = NULL;

    return current_opt;
}