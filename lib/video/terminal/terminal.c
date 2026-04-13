#include <string.h>

#include "terminal.h"
#include "text_out.h"
#include "video_out.h"
#include "os_debug.h"

// TODO, do we want this? Implement if so.
// uint8_t term_grid[TEXT_NUM_COLS][TEXT_NUM_ROWS];

uint8_t xcursor = 0;
uint8_t ycursor = 0;

void term_init(void) {
    text_init();
}

void term_putc(char symbol) {
    text_draw_char(xcursor, ycursor, symbol, false);
    
    _term_adv_cursor(1);
}

void term_draw_char(uint8_t x_cell, uint8_t y_cell, uint8_t symbol) {
    text_draw_char(x_cell, y_cell, symbol, false);
}

void term_draw_char_inv(uint8_t x_cell, uint8_t y_cell, uint8_t symbol) {
    text_draw_char(x_cell, y_cell, symbol, true);
}

uint16_t term_puts(const char * text_string) {

    uint8_t rem_cols = TEXT_NUM_COLS - xcursor;
    uint16_t string_length = strlen(text_string);
    uint8_t draw_len = rem_cols < string_length ? rem_cols : string_length;

    text_draw_string(xcursor, ycursor, text_string, draw_len);

    _term_adv_cursor(draw_len);

    return draw_len;
}

uint16_t term_draw_string(uint8_t x_cell, uint8_t y_cell, const char * text_string) {
    uint8_t rem_cols = TEXT_NUM_COLS - x_cell;
    uint16_t string_length = strlen(text_string);
    uint8_t draw_len = rem_cols < string_length ? rem_cols : string_length;

    text_draw_string(x_cell, y_cell, text_string, draw_len);

    return draw_len;
}

static void _term_adv_cursor(uint8_t spaces) {

    text_restore_cell(xcursor, ycursor);

    xcursor += spaces;
    
    if (xcursor > 79) {
        if (ycursor == 29) {
            xcursor = 79;
        } else {
            ycursor += 1;
            xcursor = 0;
        }
    }

    text_gray_cell(xcursor, ycursor);
}

void term_set_cursor(uint16_t x_cell, uint16_t y_cell) {
    if (x_cell >= TEXT_NUM_COLS || y_cell >= TEXT_NUM_ROWS) {
        debug_deposit("TERM-WARN:term_set_cursor() out of range", 0, DBG_NULL_VAR);
        return;
    }

    text_restore_cell(xcursor, ycursor);

    xcursor = x_cell;
    ycursor = y_cell;

    text_gray_cell(xcursor, ycursor);
}

void term_clear_visual_cursor(void) {
    text_restore_cell(xcursor, ycursor);
}

void term_gray_cursor(void) {
    text_gray_cell(xcursor, ycursor);
}

uint8_t term_get_xcursor(void) {
    return xcursor;
}

uint8_t term_get_ycursor(void) {
    return ycursor;
}

void term_clear_screen() {
    text_clear_screen();
}

void term_clear_prog_screen(void) {
    // start at row 1 because row 0 is controlled by main loop
    for (uint8_t row = 1; row < TERM_NUM_ROWS; row++) {
        term_clear_line(row);
    }
}

void term_clear_line(uint8_t row) {
    for (uint8_t col = 0; col < TERM_NUM_COLS; col++) {
        text_draw_char(col, row, 0x00, false);
    }
}

// DO NOT USE, may not use this
void term_backspace(void) {
    text_restore_cell(xcursor, ycursor);
    if (xcursor) { // if xcursor is NOT at 0 position
        // not at first col, same line
        xcursor = xcursor - 1;
    } else { // cursor at 0 position (far left)
        // must go back to prev line
        // BAD BROKEN: just goes to end of prev line
        xcursor = 79;
        if (ycursor) { // only goes up line if not at row 0 (top)
            ycursor = ycursor - 1;
        }
    }

    text_draw_char(xcursor, ycursor, 0x00, false);
    text_gray_cell(xcursor, ycursor);
}

// may not use this
void term_new_line(void) {
    text_restore_cell(xcursor, ycursor);

    if (ycursor < TERM_NUM_ROWS-1) {
        xcursor = 0;
        ycursor += 1;
    }

    text_gray_cell(xcursor, ycursor);
}

void term_move_line(uint16_t line_num, bool down_flag) {
    text_move_line(line_num, down_flag);
}

void term_invert_line(uint16_t line_num) {
    for (uint8_t col = 0; col < TEXT_NUM_COLS; col++) {
        text_invert_cell(col, line_num);
    }
}

void term_invert_cell(uint16_t x_cell, uint16_t y_cell) {
    text_invert_cell(x_cell, y_cell);
}

