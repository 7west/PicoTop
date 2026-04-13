#pragma once

#include "calc_types.h"








void calc_draw_all_cells(calc_cell_t **cells_ptrs, uint16_t top_ui_cell, uint16_t curr_cell);

void calc_draw_one_cell(uint8_t pos, const calc_cell_t *cells, uint8_t cell_idx, bool selected);

void calc_draw_cursor(uint16_t top_ui_cell, uint16_t curr_cell, uint16_t cursor_pos);

void calc_remove_cursor(void);

// turns double into string. Tries to remove trailing zeros and if it is an integer
//  it will print hex and bin versions of the integer
void calc_format_double(double result, char *out_msg);

// draws shortcuts, rad/deg, NAV/HELP/EDIT markers, cell count on bottom UI bar
void calc_bottom_ui_update(calc_mode_t mode, uint16_t cell_count, bool rad_mode);

void calc_update_cell_count(uint16_t cell_count);

void calc_wipe_ui_symbols(void);

void calc_wipe_main_ui(void);

void calc_draw_help_ui(void);

void calc_save_prompt(void);

// called when typing the name of the file to save/load
void calc_save_add_char(uint8_t idx, char sym);

// prints the save/loading status updates on the bottom UI bar
//  prints, then delays 750 ms. Not the best method, but it works fine
void calc_save_output(const char *status);

void calc_load_prompt(void);