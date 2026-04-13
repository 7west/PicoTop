#pragma once

#include "calc_defs.h"

typedef struct {
    char expr[CALC_EXPR_TOT_LEN+1];
    uint8_t flags;
    uint8_t len;
    char out_msg[CALC_OUT_MSG_LEN+1];
    double result;

} calc_cell_t;

typedef enum {
    CALC_NAV_MODE,
    CALC_EDIT_MODE,
    CALC_HELP_MODE,

} __attribute__ ((packed)) calc_mode_t;

typedef struct {
    calc_mode_t mode;
    uint16_t cell_count;
    uint16_t curr_cell;
    uint16_t top_ui_cell;
    uint8_t cursor_pos;
    bool rad_enabled;

} calc_state_t;