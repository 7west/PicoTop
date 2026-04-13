#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"

#include "calc_ui.h"
#include "terminal.h"
#include "calc_help.h"

// takes position in viewport (0-6) and draws a cell there
// static void calc_draw_one_cell(uint8_t pos, const calc_cell_t *cells, uint8_t cell_idx);
static void print_uint16(char *out, uint16_t val);
// draws the inverted chars to show which cell is selected
// static void calc_ui_select_cell(uint16_t top_ui_cell, uint16_t curr_cell);






void calc_draw_cursor(uint16_t top_ui_cell, uint16_t curr_cell, uint16_t cursor_pos) {
    uint8_t pos = curr_cell - top_ui_cell;
    uint8_t y = 1+ (pos * 4);

    term_set_cursor((cursor_pos % CALC_EXPR_LINE_LEN) + 6, y + (cursor_pos / CALC_EXPR_LINE_LEN));
}

void calc_remove_cursor(void) {
    term_clear_visual_cursor();
}

void calc_draw_all_cells(calc_cell_t **cells_ptrs, uint16_t top_ui_cell, uint16_t curr_cell) {

    for (uint16_t i = 0; i < CALC_CELLS_PER_WINDOW; i++) {
        if (cells_ptrs[top_ui_cell + i]->flags & CALC_CELL_FLAG_EXISTS) {
            bool selected = curr_cell - top_ui_cell == i;
            calc_draw_one_cell(i, cells_ptrs[top_ui_cell + i], top_ui_cell + i, selected);
        }
    }

    // calc_ui_select_cell(top_ui_cell, curr_cell);
}

void calc_draw_one_cell(uint8_t pos, const calc_cell_t *cell, uint8_t cell_idx, bool selected) {
    if (pos > 6) return;

    uint8_t y = 1 + (pos * 4);
    char cell_num[6] = "[   ]";
    // add cell number to cell_num
    // we show the user index 1 cells, even though in code we start counting at 0
    print_uint16(cell_num + 1, cell_idx + 1);

    term_draw_string(0, y, cell_num);

    // write expression
    uint16_t char_written = term_draw_string(6, y, cell->expr);
    if (char_written < cell->len) {
        term_draw_string(6, y + 1, cell->expr + char_written);
    }

    // write result/error message
    term_clear_line(y + 2);
    term_draw_char(6, y + 2, '=');
    term_draw_string(7, y + 2, cell->out_msg);
    term_draw_string(0, y + 3, "------------------------------------------------------------------------------------------");

    if (selected) {
        for (uint8_t i = 0; i < 5; i++) {
            term_invert_cell(i, y);
        }
    }
}

// wipes everything EXCEPT bottom UI bar (and top UI bar)
void calc_wipe_main_ui(void) {
    for (uint8_t i = 1; i < TERM_NUM_ROWS - 1; i++) {
        term_clear_line(i);
    }
}

// prints right aligned number
static void print_uint16(char *out, uint16_t val) {
    if (val > 99) {
        *out++ = (val / 100) + '0';
        val %= 100;
        *out++ = (val / 10) + '0';
    } else if (val > 9) {
        *out++ = ' ';
        *out++ = (val / 10) + '0';
    } else {
        *out++ = ' ';
        *out++ = ' ';
    }
    *out++ = (val % 10) + '0';
}

// must be called AFTER calc_ui_draw_all_cells()
//  otherwise old ui select will be there
// static void calc_ui_select_cell(uint16_t top_ui_cell, uint16_t curr_cell) {

//     // we draw the '[' for all cell slots on the viewport, so the user knows
//     //      they can add cells
//     for (uint16_t i = 0; i < CALC_CELLS_PER_WINDOW; i++) {
//         term_draw_char(0, 1 + (i * 4), '[');
//     }
    
//     for (uint8_t i = 0; i < 5; i++) {
//         term_invert_cell(i, 1 + (curr_cell - top_ui_cell) * 4);
//     }
// }

// wipes out the "----" between cells, the '=' char, and the []
//  used when you need to delete cells
void calc_wipe_ui_symbols(void) {
    for (uint16_t i = 0; i < CALC_CELLS_PER_WINDOW; i++) {
        term_clear_line(4 + (i * 4));
        term_draw_char(6, 3 + (i * 4), ' ');
        term_draw_string(0, 1 + (i * 4), "     ");
    }

}

void calc_format_double(double result, char *out_msg) {
    double mag = fabs(result);

    double epsilon = (mag > 1.0 ? mag : 1.0) * 1e-9;

    if (fabs(result - round(result)) <= epsilon) {

        if (mag < 1e18) { // regular integer
            snprintf(out_msg, CALC_ERR_MSG_LEN, "%lld", (long long)result);
        } else { // scientific notation
            snprintf(out_msg, CALC_ERR_MSG_LEN, "%.6e", result);
        }

        // print hexadecimal representation
        if (mag <= (double)UINT32_MAX) {
            uint8_t len = strlen(out_msg);
            uint32_t uval = (result < 0) ? (uint32_t)(int32_t)result : (uint32_t)result;
            snprintf(out_msg + len, CALC_OUT_MSG_LEN - len, "   0x%X", uval);

            // print binary representation
            if (mag <= 0xFFFF && uval > 0) {
                len = strlen(out_msg); // recalculate since hex is not fixed width
                char *p = out_msg + len;
                *p++ = ' '; *p++ = ' '; *p++ = ' ';
                *p++ = '0'; *p++ = 'b';
                int8_t top = 15;
                while (top > 0 && !((uval >> top) & 1)) {
                    top--;
                }
                for (int8_t i = top; i >= 0; i--) {
                    *p++ = (uval >> i) & 1 ? '1' : '0';
                }
                *p = '\0';
            }
        }

    } else {
        snprintf(out_msg, CALC_ERR_MSG_LEN, "%.13g", result);

        // strip trailing zeros after decimal point
        // necessary because %g isn't behaving like C should due to Pico SDK
        char *dot = strchr(out_msg, '.');
        if (dot) {
            char *end = out_msg + strlen(out_msg) - 1;
            while (end > dot && *end == '0') {
                end--;
            }
            if (end == dot) {
                // all decimals were zero, remove the dot too
                *end = '\0';
            } else {
                *(end + 1) = '\0';
            }
        }
    }
}

// the first _ in the line below, for use by calc_update_cell_count() to know where to write
#define CALC_CELL_NUM_X 37

void calc_bottom_ui_update(calc_mode_t mode, uint16_t cell_count, bool rad_mode) {
    term_clear_line(TERM_NUM_ROWS-1);

    term_draw_string(1, TERM_NUM_ROWS-1, "^H Help | _ Mode | ^X Exit |  Cells:___");
    term_draw_char(11, TERM_NUM_ROWS-1, 0x1C); // Enter symbol

    if (rad_mode) {
        term_draw_string(70, TERM_NUM_ROWS-1, "RAD");
    } else {
        term_draw_string(70, TERM_NUM_ROWS-1, "DEG");
    }

    if (mode == CALC_NAV_MODE) {
        term_draw_string(TERM_NUM_COLS - 5, TERM_NUM_ROWS - 1, "NAV");
    } else if (mode == CALC_EDIT_MODE) {
        term_draw_string(TERM_NUM_COLS - 5, TERM_NUM_ROWS - 1, "EDIT");
    } else if (mode == CALC_HELP_MODE) {
        term_draw_string(TERM_NUM_COLS - 5, TERM_NUM_ROWS - 1, "HELP");
    }
    term_invert_line(TERM_NUM_ROWS-1);

    calc_update_cell_count(cell_count);
}

// magic number based on the prompt written below
#define SAVE_PROMPT_LEN 47

void calc_save_prompt(void) {
    term_clear_line(TERM_NUM_ROWS-1);

    term_draw_string(1, TERM_NUM_ROWS-1, "Saving file in present directory. Enter name: ");

    term_invert_line(TERM_NUM_ROWS-1);

    term_invert_cell(SAVE_PROMPT_LEN, TERM_NUM_ROWS-1);
}

void calc_load_prompt(void) {
    term_clear_line(TERM_NUM_ROWS-1);

    // should be same char len as save_prompt above^ (or at least, less chars)
    term_draw_string(1, TERM_NUM_ROWS-1, "Enter file name to open. (Name only, no ext): ");

    term_invert_line(TERM_NUM_ROWS-1);

    term_invert_cell(SAVE_PROMPT_LEN, TERM_NUM_ROWS-1);
}

void calc_save_add_char(uint8_t idx, char sym) {
    term_draw_char_inv(idx + SAVE_PROMPT_LEN, TERM_NUM_ROWS-1, sym);
    term_invert_cell(idx + 1 + SAVE_PROMPT_LEN, TERM_NUM_ROWS-1);
}

void calc_save_output(const char *status) {
    term_clear_line(TERM_NUM_ROWS-1);
    term_draw_string(1, TERM_NUM_ROWS-1, status);
    term_invert_line(TERM_NUM_ROWS-1);

    sleep_ms(750); // this is not a great idea. but it works...
}

void calc_update_cell_count(uint16_t cell_count) {
    char cell_str[4] = "   ";

    print_uint16(cell_str, cell_count);
    term_draw_string(CALC_CELL_NUM_X, TERM_NUM_ROWS - 1, cell_str);

    for (uint8_t i = 0; i < 3; i++) {
        term_invert_cell(CALC_CELL_NUM_X + i, TERM_NUM_ROWS - 1);
    }
}

void calc_draw_help_ui(void) {
    for (uint8_t i = 0; i < TERM_NUM_ROWS - 2; i++) {
        term_draw_string(0, i + 1, CALC_HELP[i]);
    }

    for (uint8_t i = 10; i < TERM_NUM_COLS - 5; i++) {
        term_draw_char_inv(i, TERM_NUM_ROWS-1, ' ');
    }
}