// Interface for programs and utilities to do text, new lines, scrolling, etc.
//      a lot of these functions just wrap text_out.c functions...
#pragma once

#include <stdio.h>
#include "pico/stdlib.h"

// maybe hide in .c file? If not you need to do extern & .c combo
// extern uint8_t term_grid[][];

#define TERM_MOVE_LINE_DOWN true
#define TERM_MOVE_LINE_UP false

// this is not flexible. Update if resolution changes.
#define TERM_NUM_COLS 80
#define TERM_NUM_ROWS 30

// calls text_init(), which calls video_out_setup()
void term_init(void);

///////////////////////////////////////////
// CURSOR DRIVEN DRAWING:

/**
 * @brief Sets cursor position. GRAY's position.
 * 
 * Will de-gray previous position, ONLY during gray to black.
 * Graying means taking black (0x00) pixels and turning gray (0x49)
 * 
 * @param x_cell 0-79 (TERM_NUM_COLS - 1)
 * @param y_cell 0-29 (TERM_NUM_ROWS - 1)
 */
void term_set_cursor(uint16_t x_cell, uint16_t y_cell);

/**
 * @brief Removes the gray box of the cursor. Does NOT delete current cursor position
 */
void term_clear_visual_cursor(void);

/**
 * @brief Adds gray box at cursor. Needed for some backspace mechanics 
 */
void term_gray_cursor();

// returns ycursor value
uint8_t term_get_ycursor();
// returns ycursor value
uint8_t term_get_xcursor();

/**
 * @brief Puts char at cursor position and advances cursor. WILL WRAP!
 * 
 * Wrap will go to next line at pos x=0. If at last line (y=29), cursor
 * stays at x=79 (bottom right)
 * 
 * @param symbol 
 */
void term_putc(char symbol);

/**
 * @brief writes string to at current cursor position. WILL NOT WRAP. Will truncate string
 * 
 * It only writes the string chars that fit on the rest of the current row.
 * 
 * @param text_string duh
 * @return uint16_t number of chars written
 */
uint16_t term_puts(const char * text_string);

/**
 * @brief 1. moves cursor back (will wrap, badly) 2. removes that char
 * 
 * CAREFUL. Backspace at col 0 (far left) goes to prev row and end of line (far right)
 * 
 */
void term_backspace(void);

/**
 * @brief moves cursor to next line, far left
 * 
 * CAREFUL. Always puts you at col 0 (far left) of next line
 * 
 */
void term_new_line(void);

///////////////////////////////////////////
// STANDARD/UI DRIVEN DRAWING:

// just draws char at x_cell,y_cell
void term_draw_char(uint8_t x_cell, uint8_t y_cell, uint8_t symbol);

// just draws char at x_cell,y_cell with color inverted
void term_draw_char_inv(uint8_t x_cell, uint8_t y_cell, uint8_t symbol);

// draws string at x_cell,y_cell. Will NOT wrap. Returns chars written
uint16_t term_draw_string(uint8_t x_cell, uint8_t y_cell, const char * text_string);

// inverts black/white of a line
void term_invert_line(uint16_t line_num);

// inverts black/white of a single cell
void term_invert_cell(uint16_t x_cell, uint16_t y_cell);

// moves line on screen up/down. WILL overwrite destination line
void term_move_line(uint16_t line_num, bool down_flag);

//////////////////////////////////////////
// CLEARING THE SCREEN:

// All black (0x00)
void term_clear_screen();

// Writes all lines except #0 (top) to black (0x00)
void term_clear_prog_screen(void);

// writes black (0x00) for just one line
void term_clear_line(uint8_t row);

///////////////////////////////////////////
// Helper Functions:

/**
 * @brief Moves cursor 'forward' # of spaces
 * 
 * Will wrap lines. Will get stuck at last row/col (bottom right)
 */
static void _term_adv_cursor(uint8_t spaces);