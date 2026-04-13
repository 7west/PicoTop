#pragma once

#include "calc_defs.h"

typedef enum {
    NODE_NUM,
    NODE_VAR,
    NODE_BIN_OP,
    NODE_UNA_OP,
    NODE_FUNC,
} NodeType;

// op_precedence[] in eval.c follows this. Verify changes.
typedef enum {
    OP_BIT_OR = 0,
    OP_BIT_XOR,
    OP_BIT_AND,
    OP_SHIFT_L,
    OP_SHIFT_R,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_NEGATE,
    OP_BIT_NOT,
} OpType;

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
    CALC_RETURN_EVAL_TOO_DEEP,
    CALC_DIV_BY_ZERO,
    CALC_INFINITY_RESULT,
    CALC_IMAGINARY_RESULT,
    CALC_FUNCTION_DNE,
    CALC_RETURN_FUNC_ARG_COUNT_ERROR,
    CALC_RETURN_BIT_SHIFT_ERROR,


    CALC_RETURN_UNKNOWN_ERROR,
} calc_return_t;


typedef struct ASTNode {
    NodeType type;
    union {
        double number;
        char var_name[VAR_NAME_MAX+1];
        OpType op;
        uint8_t func_index;
    } data;
    struct ASTNode *left;
    struct ASTNode *right;
} ASTNode;