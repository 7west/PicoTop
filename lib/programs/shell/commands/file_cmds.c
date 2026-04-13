#include "../commands.h"
#include "vfs.h"



/////////////////////////////////////
// cmd_rm() - removes a given file
/////////////////////////////////////

bool cmd_rm_help(const out_t *out) {
    out->write(out->ctx, "deletes the file given.\n -f option to force for System files");
    return true;
}

bool cmd_rm_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_rm_help(out);
}

bool cmd_rm(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_rm_help(out);
    }

    if (argc < 2 || argc > 3) return cmd_rm_fail(out);

    out->write(out->ctx, "deleting... ");

    bool f_flag = false;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "f", &state)) != -1) {

        switch(c) {
            case 'f':
                f_flag = true;
                break;
            default:
                return cmd_rm_fail(out);
        }
    }

    const char *target_file;
    if (state.optind < argc) {
        target_file = argv[state.optind];
    } else {
        return cmd_rm_fail(out);
    }

    if (vfs_delete(target_file, f_flag)) {
        out->write(out->ctx, "file deleted");
    } else {
        out->write(out->ctx, "delete failed");
    }

    return true;
}

// TODO: What if rm * removed everything in a directory?
//          get every file name from directory (vfs_opendir & _listdir)
//          concatenate with pwd
//          pass each to vfs_delete




/////////////////////////////////////
// cmd_move() - moves or renames a file
/////////////////////////////////////

bool cmd_move_help(const out_t *out) {
    out->write(out->ctx, "moves source(1) to destination(2). Can rename files. Source and destination (if it exists) must not be open\n -f option to force for System files");
    return true;
}

bool cmd_move_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_move_help(out);
}

bool cmd_move(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_move_help(out);
    }

    if (argc < 3 || argc > 4) return cmd_move_fail(out);

    out->write(out->ctx, "moving... ");

    bool f_flag = false;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "f", &state)) != -1) {

        switch(c) {
            case 'f':
                f_flag = true;
                break;
            default:
                return cmd_move_fail(out);
        }
    }

    const char *source_file;
    const char *dest_file;
    if (state.optind < argc) {
        source_file = argv[state.optind];
    } else {
        return cmd_move_fail(out);
    }
    state.optind += 1;
    if (state.optind < argc) {
        dest_file = argv[state.optind];
    } else {
        return cmd_move_fail(out);
    }

    if (vfs_move(source_file, dest_file, f_flag)) {
        out->write(out->ctx, "move successful");
    } else {
        out->write(out->ctx, "move failed");
    }

    return true;
}


/////////////////////////////////////
// cmd_copy() - copies a file
/////////////////////////////////////

bool cmd_copy_help(const out_t *out) {
    out->write(out->ctx, "copies source(1) to destination(2). Destination must not be open if it exists");
    return true;
}

bool cmd_copy_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_copy_help(out);
}

bool cmd_copy(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_copy_help(out);
    }

    if (argc != 3) return cmd_copy_fail(out);

    out->write(out->ctx, "copying... ");

    if (vfs_copy(argv[1], argv[2])) {
        out->write(out->ctx, "copy successful");
    } else {
        out->write(out->ctx, "copy failed");
    }

    return true;
}