#pragma once

#include "calc_defs.h"

// These are for rad/deg checks performed in eval.c ast_eval()
// The first 3 functions are trig, so this is 3
#define CALC_FUNC_TRIG_FUNCS 3
// The inverse trig functions end at 7, so this is 7
#define CALC_FUNC_INV_TRIG_FUNCS 7

typedef double (*UnaryFunc)(double);
typedef double (*BinaryFunc)(double, double);

typedef struct {
    char name[VAR_NAME_MAX+1];
    uint8_t arg_count;
    uint8_t flags;
    union {
        UnaryFunc unary;
        BinaryFunc binary;
    } func;
} FuncEntry;










size_t vars_mem_size(void);

void vars_init(void *mem);


bool vars_get(const char *name, double *value);

bool vars_set(const char *name, double value);

void var_set_ans(double value);

void var_set_ans_valid(bool valid);

// bool var_is_ans_valid(void);

bool vars_clear_user(void);


bool funcs_get_idx(const char *name, uint8_t *idx);

FuncEntry* funcs_get_func(uint8_t index);

const char *funcs_get_error(void);

void funcs_clear_error(void);