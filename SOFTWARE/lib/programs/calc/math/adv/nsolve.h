#pragma once

#include "calc_types.h"

typedef struct ASTNode ASTNode;
typedef struct CalcResult calc_result_t;















calc_return_t nsolve_derivative(ASTNode *tree, const char *var, double complex at_value, calc_result_t *result);

calc_return_t nsolve_root(ASTNode *tree, const char *var, double complex initial_guess, calc_result_t *result);

calc_return_t nsolve_integrate(ASTNode *tree, const char *var, double lower, double upper, calc_result_t *result);