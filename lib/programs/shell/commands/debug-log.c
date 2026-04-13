#include "../commands.h"
#include "os_debug.h"



bool cmd_debug_log_help(const out_t *out) {
    out->write(out->ctx, "Prints all OS Debug Log entries; max of 128 entries");
    return true;
}

bool cmd_debug_log_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_debug_log_help(out);
}

bool cmd_debug_log(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_debug_log_help(out);
    }

    if (argc != 1) {
        return cmd_debug_log_fail(out);
    }

    uint32_t total_entries = debug_get_total_entries();

    if (total_entries == 0) return true;

    uint32_t entries_to_print = total_entries;
    if (entries_to_print > DEBUG_MAX_ENTRIES) {
        entries_to_print = DEBUG_MAX_ENTRIES;
    }

    uint32_t start_index = total_entries - entries_to_print;

    char entry[81] = {0};

    for (uint32_t i = 0; i < entries_to_print; i++) {
        
        debug_withdraw_entry(start_index + i, entry);

        if (!out->write(out->ctx, entry)) return true;

        if (i + 1 < entries_to_print) {
            if (!out->write(out->ctx, "\n")) return true;
        }
    }

    return true;
}