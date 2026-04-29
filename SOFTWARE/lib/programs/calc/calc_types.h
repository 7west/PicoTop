#pragma once

#include <complex.h>
#include "calc_defs.h"

typedef enum {
    CALC_RESULT_NUM,
    CALC_RESULT_STR,
    CALC_RESULT_ERR,
} calc_result_mode_t;

typedef struct CalcResult {
    calc_result_mode_t mode;
    union {
        double complex number;
        char str_out[CALC_EXPR_LINE_LEN]; // + 1 for nul, -1 for = at front of answer
    } data;
    
} calc_result_t;

typedef struct {
    char expr[CALC_EXPR_TOT_LEN+1];
    uint8_t flags;
    uint8_t len;
    calc_result_t result;
    // char out_msg[CALC_OUT_MSG_LEN+1];
    // double result;

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

typedef enum {
    CALC_RETURN_SUCCESS = 0,
    CALC_RETURN_NUMBER_PARSE_FAIL,
    CALC_RETURN_NAME_PARSE_FAIL,
    CALC_RETURN_OP_PARSE_FAIL,
    CALC_RETURN_TOKEN_FAIL,
    CALC_RETURN_AST_FAIL,
    CALC_RETURN_AST_OVERLOAD_FAIL,
    CALC_RETURN_GEN_EVAL_ERROR,
    CALC_RETURN_VAR_GET_ERROR,
    CALC_RETURN_VAR_SET_ERROR,
    CALC_RETURN_EVAL_TOO_DEEP,
    CALC_DIV_BY_ZERO,
    CALC_INFINITY_RESULT,
    // CALC_IMAGINARY_RESULT,
    CALC_COMPLEX_TO_INT_ERROR,
    CALC_FUNCTION_DNE,
    CALC_RETURN_FUNC_ARG_COUNT_ERROR,
    CALC_RETURN_BIT_SHIFT_ERROR,
    CALC_NUM_SOLVE_DERIV_FAIL,
    CALC_NUM_SOLVE_LOOP_FAIL,


    CALC_RETURN_UNKNOWN_ERROR,
} calc_return_t;