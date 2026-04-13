#pragma once

#include "calc_types.h"


// add one character at cursor position
bool calc_edit_insert(char symbol, calc_cell_t *cell, uint8_t *cursor_pos);

// backspace a char, moves cursor
bool calc_edit_bksp(calc_cell_t *cell, uint8_t *cursor_pos);

// delete a char, does not move cursor
bool calc_edit_del(calc_cell_t *cell, uint8_t *cursor_pos);

