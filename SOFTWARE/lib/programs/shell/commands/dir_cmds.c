#include "../commands.h"
#include "vfs.h"

#define LS_LONGEST_STRING 16 // timestamp YYYY-MM-DD HH:SS



/////////////////////////////////////
// cmd_pwd() - print present working directory
/////////////////////////////////////

bool cmd_pwd_help(const out_t *out) {
    out->write(out->ctx, "prints present working directory");
    return true;
}

bool cmd_pwd_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_pwd_help(out);
}

bool cmd_pwd(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_pwd_help(out);
    }

    if (argc != 1) return cmd_pwd_fail(out);

    out->write(out->ctx, vfs_getpwd());

    return true;
}

/////////////////////////////////////
// cmd_chdir() - change working directory
/////////////////////////////////////

bool cmd_chdir_help(const out_t *out) {
    out->write(out->ctx, "change directory to given path");
    return true;
}

bool cmd_chdir_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_chdir_help(out);
}

bool cmd_chdir(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_chdir_help(out);
    }

    if (argc != 2) return cmd_chdir_fail(out);

    if (strlen(argv[1]) > VFS_MAX_PATH_LEN) {
        out->write(out->ctx, "CMD FAIL: maximum path length exceeded");
        return true;
    }

    if (vfs_chdir(argv[1])) {
        out->write(out->ctx, "pwd:");
        out->write(out->ctx, vfs_getpwd());
    } else {
        out->write(out->ctx, "CMD FAIL: path does not exist");
    }
    return true;
}

/////////////////////////////////////
// cmd_ls() - list files & subdirectories in current directory
/////////////////////////////////////

bool cmd_ls_help(const out_t *out) {
    out->write(out->ctx, "Lists files & directories in present working directory\n ls [-l/a]\n  -l = long form. -a = shows hidden entries. Can be combined");
    return true;
}

bool cmd_ls_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_ls_help(out);
}

void cmd_ls_names(const out_t *out, bool all_flag) {
    fat_dir_list_t dir;
    vfs_dir_entry_t entry;

    if (vfs_opendir(vfs_getpwd(), &dir)) {

        uint8_t entry_counter = 0;

        while (vfs_listdir(&dir, FS_FILE_NAME, &entry, all_flag)) {
            // printf("ls printing entry %u\n", entry_counter);
            out->write(out->ctx, entry.name);
            entry_counter += 1;
            
            if (entry_counter & 3) { // same as % 4
                out->write(out->ctx, "     ");
            } else {
                out->write(out->ctx, "\n");
            }
        }
    } else {
        out->write(out->ctx, "Listing directory entries failed");
    }
}

void cmd_ls_long(const out_t *out, bool all_flag) {
    fat_dir_list_t dir;
    vfs_dir_entry_t entry;

    if (vfs_opendir(vfs_getpwd(), &dir)) {

        uint8_t entry_counter = 0;

        while (vfs_listdir(&dir, FS_FILE_NAME_SIZE_MTIME_ATTR, &entry, all_flag)) {

            out->write(out->ctx, entry.name);
            out->write(out->ctx, " ");
            out->write(out->ctx, entry.size);
            out->write(out->ctx, " ");
            out->write(out->ctx, entry.attr);
            out->write(out->ctx, " ");
            out->write(out->ctx, entry.ts);
            

            out->write(out->ctx, "\n");

            entry_counter += 1;

        }

        char entry_string[12];
        snprintf(entry_string, 12, "%u entries", entry_counter);
        out->write(out->ctx, entry_string);

    } else {
        out->write(out->ctx, "Listing directory entries failed");
    }

}

bool cmd_ls(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) 
            || (strcmp(argv[1], "help") == 0))) {
        return cmd_ls_help(out);
    }

    if (argc > 2) return cmd_ls_fail(out); 

    bool a_flag = false;
    bool l_flag = false;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "la", &state)) != -1) {

        switch(c) {
            case 'a':
                a_flag = true;
                break;
            case 'l':
                l_flag = true;
                break;
            default:
                return cmd_ls_fail(out);
        }
    }

    if (l_flag) {
        cmd_ls_long(out, a_flag);
    } else {
        cmd_ls_names(out, a_flag);
    }

    return true;
}

/////////////////////////////////////
// cmd_mkdir() - makes a directory
/////////////////////////////////////

bool cmd_mkdir_help(const out_t *out) {
    out->write(out->ctx, "Creates a directory in the present working directory");
    return true;
}

bool cmd_mkdir_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_mkdir_help(out);
}

bool cmd_mkdir(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_mkdir_help(out);
    }

    if (argc != 2) {
        return cmd_mkdir_fail(out);
    }

    if (vfs_mkdir(argv[1])) {
        out->write(out->ctx, "Directory created");
    } else {
        out->write(out->ctx, "Failed to make directory");
    }
    return true;
}

/////////////////////////////////////
// cmd_rmdir() - makes a directory
/////////////////////////////////////

bool cmd_rmdir_help(const out_t *out) {
    out->write(out->ctx, "Removes given directory in present working directory. Must be empty.\n -f option to force for System files");
    return true;
}

bool cmd_rmdir_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_rmdir_help(out);
}

bool cmd_rmdir(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_rmdir_help(out);
    }

    if (argc < 2 || argc > 3) {
        return cmd_rmdir_fail(out);
    }

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
                return cmd_rmdir_fail(out);
        }
    }

    const char *target_file;
    if (state.optind < argc) {
        target_file = argv[state.optind];
    } else {
        return cmd_rmdir_fail(out);
    }

    if (vfs_rmdir(target_file, f_flag)) {
        out->write(out->ctx, "Directory removed");
    } else {
        out->write(out->ctx, "Failed to remove directory. Is it empty? Is it real?");
    }
    return true;
}