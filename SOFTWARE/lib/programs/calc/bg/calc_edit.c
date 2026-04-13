#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "calc_edit.h"







bool calc_edit_insert(char symbol, calc_cell_t *cell, uint8_t *cursor_pos) {
    if (cell->len == CALC_EXPR_TOT_LEN-1) return false;

    memmove(cell->expr + *cursor_pos + 1, cell->expr + *cursor_pos, cell->len - *cursor_pos);
    cell->expr[*cursor_pos] = symbol;
    *cursor_pos += 1;
    cell->len += 1;

    // ' ' vice \0 so terminal writes a blank, otherwise it would stop and not wipe old chars
    cell->expr[cell->len] = ' ';
    cell->flags |= CALC_CELL_FLAG_OCCUPIED;
    cell->flags |= CALC_CELL_FLAG_MODIFIED;

    return true;
}

bool calc_edit_bksp(calc_cell_t *cell, uint8_t *cursor_pos) {
    if (cell->len == 0) return false;
    if (*cursor_pos == 0) return false;

    memmove(cell->expr + *cursor_pos - 1, cell->expr + *cursor_pos, cell->len - *cursor_pos);
    *cursor_pos -= 1;
    cell->len -= 1;

    // ' ' vice \0 so terminal writes a blank, otherwise it would stop and not wipe old chars
    cell->expr[cell->len] = ' ';

    if (cell->len == 0) cell->flags &= ~(CALC_CELL_FLAG_OCCUPIED);
    cell->flags |= CALC_CELL_FLAG_MODIFIED;

    return true;
}

bool calc_edit_del(calc_cell_t *cell, uint8_t *cursor_pos) {
    if (cell->len == 0) return false;
    if (*cursor_pos == cell->len) return false;

    memmove(cell->expr + *cursor_pos, cell->expr + *cursor_pos + 1, cell->len - *cursor_pos - 1);
    cell->len -= 1;

    // ' ' vice \0 so terminal writes a blank, otherwise it would stop and not wipe old chars
    cell->expr[cell->len] = ' ';

    if (cell->len == 0) cell->flags &= ~(CALC_CELL_FLAG_OCCUPIED);
    cell->flags |= CALC_CELL_FLAG_MODIFIED;

    return true;
}