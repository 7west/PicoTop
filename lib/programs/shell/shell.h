#pragma once

#include "prog.h"

#define SHELL_CMD_LENGTH 256
#define SHELL_MAX_ARGS 16



typedef bool (*write_fn)(void *ctx, const char *s);

typedef struct shell_out {
    write_fn write;
    void *ctx;
} out_t;


typedef bool (*cmd_fn)(uint8_t argc, const char ** argv, const out_t *out);

typedef struct {
    const char *name;
    cmd_fn run;
} shell_cmd_t;






extern const prog_vtable_t shell_prog;
/**
 * @brief Draws UI & zeros all necessary counters and buffers 
 */
void shell_enter(void);

// does nothing
void shell_exit(void);

/**
 * @brief Processes key inputs and calls other functions
 * 
 * @param key All characters, delete, backspace, arrow keys, enter
 */
void shell_on_key(key_event_t key);

// unused currently
void shell_tick(void);

////////////////////////

/**
 * @brief takes cmd_string, finds command, and passes cmd_argv to command
 * 
 * @return true - command may have failed, but user notified on terminal
 * @return false - command not found, shell must notify user
 */
static bool _shell_execute_cmd(void);

/**
 * @brief called to handle "help command" in shell
 * 
 * @param argc number of arguments
 * @return true successfully executed help
 * @return false never really happens
 */
static bool _shell_cmd_help(uint8_t argc);

/**
 * @brief callback for commands to print to terminal through shell
 * 
 * If you perform multiple _shell_write()s, detect return value. A false means the user
 * does NOT want anymore output
 * 
 * @param ctx context, unused currently, but may be used by other callbacks
 * @param s string printed. must be null-terminated
 * @return false - not bad, just means user requested writing to stop
 */
static bool _shell_write(void *ctx, const char *s);

// draws arrows for scrolling guidance on Top UI Bar
void _shell_draw_scroll_arrows(void);

// removes the arrows above
void _shell_clear_scroll_arrows(void);

////////////////////////

// clears terminal of all commands for that fresh clean look
bool shell_cmd_clear(uint8_t argc, const char ** argv, const out_t *out);

////////////////////////

// uses term_move_line() to shift all lines up by # lines
//      will not touch top UI bar
static void _shell_term_scroll(uint8_t lines);

// puts char with term_putc() (which will wrap) but checks to scroll
static void _shell_term_putc(char symbol);

// writes string to terminal. Will wrap & scroll if string is too long
static void _shell_term_puts(const char *text_string);

// puts a char on terminal, not where cursor is
static void _shell_term_draw_char(uint8_t x_cell, uint8_t y_cell, uint8_t symbol);

/**
 * @brief removes char before idx (cursor usually) and shifts cursor left
 * DOES NOT MOVE IDX
 * 
 * It's pretty safe, but it does a lot: shifts cmd_string chars, 
 * calls term_backspace (which moves cursor), prints new cmd_string to the right
 */
static void _shell_term_backspace(uint16_t idx);

/**
 * @brief removes char at idx (cursor usually). DOES NOT MOVE IDX
 * 
 * Also pretty safe, but also does a lot: shifts cmd_string chars, prints
 * new cmd_string to the right
 */
static void _shell_term_delete(uint16_t idx);

// new line with cursor, calls term_new_line() after scrolling (if needed)
static void _shell_term_new_line(void);

// used by arrow keys, can wrap
static void _shell_term_cursor_left(void);

// used by arrow keys, can wrap
static void _shell_term_cursor_right(void);

/////////////////////////

// number go down, backward in time
static uint8_t _shell_cmd_hist_prev(uint8_t index);

// number go up, forward in time
static uint8_t _shell_cmd_hist_next(uint8_t index);

// calls _shell_term_delete() x times then _shell_term_backspace() x times
// DOES ZERO cmd_string_idx & cmd_string_len. VERY RARE
//      usually the shell_on_key() handles this
static void _shell_wipe_current_cmd(void);

// returns the 'command history index from the newest history item' offset
//  as we move hist_nav and navigate, need to know actual index in cmd_string_history[]
static uint8_t _cmd_hist_index_from_newest(uint8_t offset_from_newest);

/**
 * @brief Wipes current typed command, clears cmd_string, loads new cmd_string from line, 
 * prints it, and sets cmd_string_idx & cmd_string_len
 * 
 * @param line usually from cmd_string_history[]
 */
static void _load_cmd_line(const char *line);

