#pragma once

#include "calc_types.h"

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




typedef struct ASTNode {
    NodeType type;
    union {
        double complex number;
        char var_name[VAR_NAME_MAX+1];
        OpType op;
        uint8_t func_index;
    } data;
    struct ASTNode *left;
    struct ASTNode *right;
} ASTNode;