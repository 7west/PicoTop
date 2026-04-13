
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "shell.h"
#include "commands.h"
#include "terminal.h"
#include "os_debug.h"

#define printf(...) ((void)0)

#define SHELL_CMD_HISTORY_LEN 10 // * 256 bytes of memory for history
#define SHELL_SCROLL_LINE_AMOUNT 1 //max of 20

// TODO: debug_deposit statements?

static uint16_t cmd_string_idx = 0;
static uint16_t cmd_string_len = 0;
static char cmd_string[SHELL_CMD_LENGTH] = {0x00};

static int8_t hist_nav = -1;
static uint8_t hist_count = 0;
static uint8_t cmd_history_idx = 0; // usually points to the next available slot
static char cmd_string_saved[SHELL_CMD_LENGTH] = {0x00};
static char cmd_string_history[SHELL_CMD_HISTORY_LEN][SHELL_CMD_LENGTH] = { 0x00 };

static const char *cmd_argv[SHELL_MAX_ARGS];

static uint8_t pager_lines_left;
static bool pager_active;

// ==================================================
//      MASTER LIST OF COMMANDS
// --------------------------------------------------
static const shell_cmd_t cmds[] = {

    {"timeset", cmd_timeset},
    {"clear", shell_cmd_clear},
    {"history", shell_cmd_history},
    {"debug-log", cmd_debug_log},
    {"dl", cmd_debug_log},
    {"cd", cmd_chdir},
    {"pwd", cmd_pwd},
    {"ls", cmd_ls},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"sd-format", cmd_sd_format},
    {"sd-mount", cmd_sd_mount},
    {"sd-unmount", cmd_sd_unmount},
    {"sd-usage", cmd_sd_usage},
    {"rm", cmd_rm},
    {"cp", cmd_copy},
    {"mv", cmd_move},
    {"battv", cmd_battv},
    {"beep", cmd_beep},
    {"uedit", cmd_uedit},
    {"hexdump", cmd_hexdump},
    {"gpio", cmd_gpio},
    {"pinout", cmd_pinout},
    {"car-kill", cmd_car_kill},
    {"tester", cmd_test},
    {"attr", cmd_attr},
    {"vault-setup", cmd_vault_setup},
    {"vault-encrypt", cmd_vault_encrypt},
    {"vault-decrypt", cmd_vault_decrypt},
    {"calc", cmd_calc},
    {"hangman", cmd_hangman},




    {NULL, NULL},
};

const prog_vtable_t shell_prog = {
    .name = "shell",
    .enter = shell_enter,
    .exit = shell_exit,
    .on_key = shell_on_key,
    .tick = shell_tick,
    .periodic = NULL,
};

static const out_t shell_term_writer = {
    .ctx = NULL,
    .write = _shell_write,
};



////////////////////////////////////
// Prog Functions
////////////////////////////////////

void shell_enter(void) {

    term_clear_prog_screen();
    term_set_cursor(0,1);
    _shell_term_putc('>');

    cmd_string_idx = 0;
    cmd_string_len = 0;

    cmd_history_idx = 0;
    hist_nav = -1;
    hist_count = 0;
    memset(cmd_string_saved, 0x00, SHELL_CMD_LENGTH);
    memset(cmd_string, 0x00, SHELL_CMD_LENGTH);

    // open a log file for shell history
    //  load cmd_string_history[] with last 10 commands
    //  move cmd_history_idx to 9?
    //  move hist_count to 10?

    debug_deposit("SHEL-INFO-shell program started successfully", 0, DBG_NULL_VAR);
}

void shell_exit(void) {

    // move command history to log file?



}

void shell_tick(void) {

    // write commands history to log here?

}

void shell_on_key(key_event_t key) {
    printf("shell_on_key START\n");

    if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == 'c') {
        // print ^C
        _shell_term_putc('^');
        _shell_term_putc('C');

        // update terminal and reset cmd_string
        _shell_term_new_line();
        _shell_term_putc('>');
        memset(cmd_string, 0x00, SHELL_CMD_LENGTH);
        cmd_string_idx = 0;
        cmd_string_len = 0;

    } else if (key.keycode >= ' ' && key.keycode <= '~' && 
        cmd_string_len < SHELL_CMD_LENGTH-3) { //key is a letter
            // -3 because we need to preserve terminating null (0x00)

        printf("entered: %c\n", key.keycode);
        cmd_string[cmd_string_idx] = key.keycode;
        cmd_string_idx += 1;
        cmd_string_len = cmd_string_idx > cmd_string_len ? cmd_string_idx : cmd_string_len;

        _shell_term_putc(key.keycode);

        if (hist_nav != -1) hist_nav = -1;

    } else if (key.keycode == KBD_BASCII_BACKSPACE && cmd_string_idx > 0) { // Backspace
        printf("backspace\n");
        _shell_term_backspace(cmd_string_idx);

        if (cmd_string_len == 0) return;
        cmd_string_idx -= 1;
        cmd_string_len -= 1;

        if (hist_nav != -1) hist_nav = -1;

    } else if (key.keycode == KBD_BASCII_DELETE && cmd_string_len > 0 && cmd_string_idx < cmd_string_len) {
        printf("delete\n");
        _shell_term_delete(cmd_string_idx);

        cmd_string_len -= 1;
        // cmd_string_idx = cmd_string_idx > cmd_string_idx ? cmd_string_len : cmd_string_idx;
        
        if (hist_nav != -1) hist_nav = -1;
    
    } else if (key.keycode == KBD_BASCII_ARROW_LEFT && cmd_string_idx > 0) {
        _shell_term_cursor_left();
        cmd_string_idx -= 1;

        if (hist_nav != -1) hist_nav = -1;

    } else if (key.keycode == KBD_BASCII_ARROW_RIGHT && 
        cmd_string[cmd_string_idx] != 0x00 && cmd_string_idx < SHELL_CMD_LENGTH-2) {

        _shell_term_cursor_right();
        cmd_string_idx += 1;

        if (hist_nav != -1) hist_nav = -1;

    } else if (key.keycode == KBD_BASCII_ARROW_UP && hist_count != 0) {

        // first time entering history, save current line
        if (hist_nav == -1) {
            strcpy(cmd_string_saved, cmd_string);
            hist_nav = 0; // start navigation

        } else { // already navigating
            // move older (bounded by hist_count-1)
            if (hist_nav < (int8_t)(hist_count - 1)) hist_nav += 1;
        }

        uint8_t ring_idx = _cmd_hist_index_from_newest((uint8_t)hist_nav);
        _load_cmd_line(cmd_string_history[ring_idx]);

    } else if (key.keycode == KBD_BASCII_ARROW_DOWN && hist_nav != -1) {
        
        if (hist_nav == 0) { // leaving history, restore saved cmd line
            hist_nav = -1;
            _load_cmd_line(cmd_string_saved);
        } else { // move newer
            hist_nav -= 1;
            uint8_t ring_idx = _cmd_hist_index_from_newest((uint8_t) hist_nav);
            _load_cmd_line(cmd_string_history[ring_idx]);
        }

    } else if (key.keycode == KBD_BASCII_NEW_LINE) { // Enter
        printf("Enter\n");

        if (cmd_string_len > 0) {
            
            uint8_t prev = _shell_cmd_hist_prev(cmd_history_idx);
            // second condition: don't store sequential duplicates
            if (hist_count == 0 || (strcmp(cmd_string_history[prev], cmd_string) != 0)) {

                memset(cmd_string_history[cmd_history_idx], 0x00, SHELL_CMD_LENGTH);
                strcpy(cmd_string_history[cmd_history_idx], cmd_string);

                cmd_history_idx = _shell_cmd_hist_next(cmd_history_idx);
                if (hist_count < SHELL_CMD_HISTORY_LEN) hist_count += 1;
            }
        }

        // reset history navication on command submit
        hist_nav = -1;
        memset(cmd_string_saved, 0x00, SHELL_CMD_LENGTH);

        // submit cmd_string for parsing
        //      command runs, maybe outputs back text, finishes
        if(cmd_string_len > 0 && !_shell_execute_cmd()) {
            _shell_term_puts("Command not found");
        }

        // if a new program is launched, we do NOT want to write to terminal
        if (prog_get_active() == &shell_prog) {
            // update terminal and reset cmd_string
            _shell_term_new_line();
            _shell_term_putc('>');
            memset(cmd_string, 0x00, SHELL_CMD_LENGTH);
            cmd_string_idx = 0;
            cmd_string_len = 0;
        }
    }
    printf("shell_on_key END\n");
}

/////////////////////////////////////
// Command Execution
/////////////////////////////////////

// false is always "cmd not found"
// true could have failed, but user notified of correction
static bool _shell_execute_cmd(void) {
    uint8_t argc = 0;
    char *cmd_ptr = cmd_string;

    printf("cmd string: %s\n", cmd_string);
    printf("cmd_string_idx = %u\n", cmd_string_idx);
    printf("cmd_string_len = %u\n", cmd_string_len);

    while (*cmd_ptr != '\0' && cmd_ptr - cmd_string < cmd_string_len) {
        while (*cmd_ptr == ' ') cmd_ptr += 1; // skips leading spaces

        if (*cmd_ptr == '\0') break; // hit end of entered characters

        // save first real char in cmd_argv array
        cmd_argv[argc] = cmd_ptr;
        argc += 1;

        // blitz through anything not NULL or SPACE
        while (*cmd_ptr != '\0' && *cmd_ptr != ' ' && cmd_ptr - cmd_string < cmd_string_len) {
            cmd_ptr += 1;
        }

        // replace SPACE with NULL for string parsing at command side
        if (*cmd_ptr != '\0') {
            *cmd_ptr = '\0';
            cmd_ptr += 1;
        }

    }
    // fail if argc too big
    if (argc > SHELL_MAX_ARGS) return false;
    printf("number of args: %d\n", argc);

    _shell_term_new_line();

    // prep work for _shell_write()
    // pager_lines_left = TERM_NUM_ROWS-1 - term_get_ycursor();
    pager_lines_left = TERM_NUM_ROWS-1 - 2; // 2 reserved rows for Top UI Bar & line with cmd on it
    pager_active = false;

    // check if cmds[cmd_table_idx] == "help"
    if (strcmp(cmd_argv[0], "help") == 0) {
        printf("calling help\n");
        return _shell_cmd_help(argc);
    }

    // search through command table
    int32_t cmd_table_idx = 0;
    while (strcmp(cmd_argv[0], cmds[cmd_table_idx].name) != 0) {

        // reached end of table
        if (cmds[cmd_table_idx].name == NULL) {
            cmd_table_idx = -1;
            break;
        }
        cmd_table_idx += 1;
    }

    // fail, cmd not found
    if (cmd_table_idx < 0) {
        return false;
    }

    printf("calling command: %s\n", cmds[cmd_table_idx].name);

    // call command
    return cmds[cmd_table_idx].run(argc, cmd_argv, &shell_term_writer);
    // _shell_write gets called here before command returns control to shell
}

static bool _shell_cmd_help(uint8_t argc) {
    int32_t cmd_table_idx = 0;

    if (argc == 1) { // print list of all available commands

        _shell_write(NULL, "Available commands: ");

        while (cmds[cmd_table_idx].name != NULL) {

            _shell_write(NULL, cmds[cmd_table_idx].name);
            cmd_table_idx += 1;

            if (cmds[cmd_table_idx].name != NULL) _shell_write(NULL, ", ");
        }

        return true;
    }


    while (strcmp(cmd_argv[1], cmds[cmd_table_idx].name) != 0) {

        // reached end of table
        if (cmds[cmd_table_idx].name == NULL) {
            cmd_table_idx = -1;
            break;
        }
        cmd_table_idx += 1;
    }

    // fail, cmd not found
    if (cmd_table_idx < 0) {
        return false;
    }

    cmd_argv[1] = "-h";

    return cmds[cmd_table_idx].run(2, cmd_argv, &shell_term_writer);
}

static bool _shell_write(void *ctx, const char *s) {
    uint32_t s_idx = 0;
    uint32_t s_len = strlen(s);

    if ((s_len > 0) && (term_get_xcursor() == (TERM_NUM_COLS-1)) 
        && (term_get_ycursor() == (TERM_NUM_ROWS-1))) {

        // printf("prework line check\n");

        if (pager_lines_left == 0) {
            pager_active = true;
        }
    }
    
    while (s_idx < s_len) {
        if (pager_active) {

            _shell_draw_scroll_arrows();
            key_event_t key;

            while (1) {

                if(!keyboard_queue_pop(&key)) {
                    __wfi();
                    continue;
                }

                if (key.keycode == KBD_BASCII_ARROW_DOWN || key.keycode == KBD_BASCII_NEW_LINE) {
                    pager_lines_left = 1;
                    pager_active = false;

                    _shell_clear_scroll_arrows();
                    if (s[s_idx] != '\n') _shell_term_new_line();
                    break;
                }

                if ((key.keycode == 'q') || 
                    (((key.modifiers & KBD_CTRL_BIT) == KBD_CTRL_BIT) && key.keycode == 'c')) {

                    _shell_clear_scroll_arrows();
                    return false;
                }
            }
        }

        // explicit catch for \n
        if (s[s_idx] == '\n') {
            if (pager_lines_left == 0) {
                pager_active = true;
                continue;
            }
            _shell_term_new_line();
            pager_lines_left -= 1;
            s_idx += 1;
            continue;
        }

        uint8_t x_before = term_get_xcursor();

        // print one char
        term_putc(s[s_idx]);
        s_idx += 1;

        // if we were at 
        if ((x_before == (TERM_NUM_COLS-1)) && (s[s_idx] != '\n')) {

            if (pager_lines_left == 0) {
                pager_active = true;
                continue;
            }
            if (term_get_xcursor() != 0) _shell_term_new_line();
            pager_lines_left -= 1;
        }
    }

    return true;
}

void _shell_draw_scroll_arrows(void) {
    term_invert_line(0);
    term_draw_char(30, 0, 0x19); //0x19 is down arrow
    term_draw_string(31, 0, " scroll   q quit");
    term_invert_line(0);
}

void _shell_clear_scroll_arrows(void) {
    term_invert_line(0);
    term_draw_string(30, 0, "                 ");
    term_invert_line(0);
}

/////////////////////////////////////
// Shell specific commands
/////////////////////////////////////

bool shell_cmd_clear(uint8_t argc, const char ** argv, const out_t *out) {
    term_clear_prog_screen();
    term_set_cursor(0,0);

    return true;
}

bool shell_cmd_history(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        out->write(out->ctx, "Prints command history; past 10 commands");
        return true;
    }
    
    uint8_t index;
    // set index to the oldest item in history
    if (hist_count == SHELL_CMD_HISTORY_LEN) {
        index = cmd_history_idx;
    } else {
        index = 0;
    }
    
    for (uint8_t i = 0; i < hist_count; ) {
        out->write(out->ctx, cmd_string_history[index]);
        index = _shell_cmd_hist_next(index);
        i += 1;
        if (i < hist_count) out->write(out->ctx, "\n");
    }

    return true;
}



/////////////////////////////////////
// Terminal write filter
/////////////////////////////////////

static void _shell_term_scroll(uint8_t lines) {
    for (uint8_t scroll_dist = 0; scroll_dist < lines; scroll_dist++) {
        for (uint8_t row = 2; row < TERM_NUM_ROWS; row++) {

            term_move_line(row, false);
        }
    }
}

static void _shell_term_putc(char symbol) {
    uint8_t scroll_lines = SHELL_SCROLL_LINE_AMOUNT; 

    if (term_get_xcursor() == TERM_NUM_COLS-1) { // at end of line
        if (term_get_ycursor() == TERM_NUM_ROWS-1) { // at bottom of screen
            term_clear_visual_cursor();
            _shell_term_scroll(scroll_lines);
            term_set_cursor(0, TERM_NUM_ROWS-1 - scroll_lines);
        }
    } 

    term_putc(symbol);
}

static void _shell_term_draw_char(uint8_t x_cell, uint8_t y_cell, uint8_t symbol) {
    term_draw_char(x_cell, y_cell ,symbol);
}

static void _shell_term_puts(const char *text_string) {

    uint16_t total_len = strlen(text_string);
    uint16_t chars_written = 0;

    while (chars_written < total_len) {

        chars_written += term_puts(text_string + chars_written);
        
        if (chars_written < total_len) {
            _shell_term_new_line();
        }
    }
}

static void _shell_term_backspace(uint16_t idx) {

    if (cmd_string_len == 0) return;
    
    if (idx > cmd_string_len) idx = cmd_string_len;
    if (idx == 0) return;

    uint16_t delete_position = idx - 1;
    
    // first shift cmd_string to the left one, eating the char BEFORE cursor position (idx)
    for (uint16_t i = delete_position; i < cmd_string_len; i++) {
        cmd_string[i] = cmd_string[i + 1];
    }
    // safety to make sure null termination (should be redundant)
    cmd_string[cmd_string_len - 1] = '\0';

    // move cursor and wipe char from terminal
    term_backspace();

    uint8_t x_cell = term_get_xcursor();
    uint8_t y_cell = term_get_ycursor();
    // redraw chars at cursor for rest of cmd_string
    //      does not move cursor, uses text_draw_char() through terminal.c
    for (uint16_t i = delete_position; i < cmd_string_len; i++) {

        _shell_term_draw_char(x_cell, y_cell, cmd_string[i]);

        if (x_cell == TERM_NUM_COLS-1) { // at far right
            x_cell = 0;
            y_cell += 1;
            // we do not need to check for y_cell and row because we just backspaced
            //      the string already was on screen. One fewer char will also always be on screen
        } else {
            x_cell += 1;
        }
    }

    // clear last char on screen (should be redudant)
    _shell_term_draw_char(x_cell, y_cell, 0x00); // 0x00, NOT ' ' so we keep null terminator

    // gray cursor position again
    term_gray_cursor();
}

static void _shell_term_delete(uint16_t idx) {

    if (cmd_string_len == 0) return;

    if (idx > cmd_string_len) idx = cmd_string_len;
    if (idx == cmd_string_len) return;

    // first shift cmd_string to the left one, eating the char AT cursor position (idx)
    for (uint16_t i = idx; i < cmd_string_len; i++) {
        cmd_string[i] = cmd_string[i + 1];
    }
    // safety to make sure null termination (should be redundant)
    cmd_string[cmd_string_len - 1] = '\0';

    // remove the cursor
    term_clear_visual_cursor();

    uint8_t x_cell = term_get_xcursor();
    uint8_t y_cell = term_get_ycursor();
    // redraw chars from cursor to end of cmd string
    //      does not move cursor, uses text_draw_char() through terminal.c
    for (uint16_t i = idx; i < cmd_string_len; i++) {

        _shell_term_draw_char(x_cell, y_cell, cmd_string[i]);

        if (x_cell == TERM_NUM_COLS-1) { // at far right
            x_cell = 0;
            y_cell += 1;
            // we do not need to check for y_cell and row because we just backspaced
            //      the string already was on screen. One fewer char will also always be on screen
        } else {
            x_cell += 1;
        }
    }

    // clear last char on screen (should be redudant)
    _shell_term_draw_char(x_cell, y_cell, 0x00); // 0x00, NOT ' ' so we keep null terminator

    //gray cursor position again
    term_gray_cursor();
}

static void _shell_term_new_line(void) {
    uint8_t scroll_lines = SHELL_SCROLL_LINE_AMOUNT; //max of 20

    if (term_get_ycursor() == TERM_NUM_ROWS-1) { // at last line
        term_clear_visual_cursor();
        _shell_term_scroll(scroll_lines);
        term_set_cursor(0, TERM_NUM_ROWS-1 - scroll_lines);
    }

    term_new_line();
}

static void _shell_term_cursor_left(void) {
    if (term_get_xcursor() != 0) { // not at far left
        term_set_cursor(term_get_xcursor() - 1, term_get_ycursor());
    } else {
        if (term_get_ycursor() > 1) {
            term_set_cursor(TERM_NUM_COLS-1, term_get_ycursor() - 1);
        }
    }
}

static void _shell_term_cursor_right(void) {
    if (term_get_xcursor() < TERM_NUM_COLS-1) { // not at far right
        term_set_cursor(term_get_xcursor() + 1, term_get_ycursor());
    } else { // at far right
        if (term_get_ycursor() < TERM_NUM_ROWS-1) {
            term_set_cursor(0, term_get_ycursor() + 1);
        }
    }
}


/////////////////////////////////////
// Command History Helper Functions
/////////////////////////////////////

static uint8_t _shell_cmd_hist_next(uint8_t index) {
    return index == SHELL_CMD_HISTORY_LEN-1 ? 0 : index + 1;
}

static uint8_t _shell_cmd_hist_prev(uint8_t index) {
    return index == 0 ? SHELL_CMD_HISTORY_LEN-1 : index - 1;
}

static void _shell_wipe_current_cmd(void) {

    if (cmd_string_idx > cmd_string_len) {
        cmd_string_idx = cmd_string_len;
    }

    while (cmd_string_idx < cmd_string_len) {
        _shell_term_delete(cmd_string_idx);
        cmd_string_len -= 1;
    }

    while (cmd_string_idx > 0) {
        _shell_term_backspace(cmd_string_idx);
        cmd_string_idx -= 1;
        cmd_string_len -= 1;
    }

    cmd_string_idx = 0;
    cmd_string_len = 0;
}

static uint8_t _cmd_hist_index_from_newest(uint8_t offset_from_newest) {
    // offset == 0, newest. offset == 1, prev... etc

    // get newest index
    uint8_t idx = _shell_cmd_hist_prev(cmd_history_idx);

    // walk backwards by offset_from_newest
    while (offset_from_newest--) {
        idx = _shell_cmd_hist_prev(idx);
    }
    return idx;
}

static void _load_cmd_line(const char *line) {
    _shell_wipe_current_cmd();
    memset(cmd_string, 0x00, SHELL_CMD_LENGTH);
    strcpy(cmd_string, line);

    _shell_term_puts(cmd_string);
    cmd_string_idx = (uint16_t)strlen(cmd_string);
    cmd_string_len = cmd_string_idx;
}




