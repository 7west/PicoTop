#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>

#include "nsolve.h"
#include "../eval.h"
#include "../ast.h"
#include "../vars.h"


#define NUMERICAL_H 1e-5
#define ROOT_SOLVE_LOOP_MAX 100
#define NUMERICAL_SUBINTERVALS 100 // MUST BE EVEN!

#define ERR_SEND(dst, lit) do {                         \
    _Static_assert(sizeof(lit) <= CALC_ERR_MSG_LEN + 1, \
                "error message too long");              \
    strcpy(dst, lit);                                   \
} while(0)


static bool ast_return_check(calc_return_t *ret_val, double complex *result, char *str_out);







calc_return_t nsolve_derivative(ASTNode *tree, const char *var, double complex at_value, calc_result_t *result) {

    // === Saving off or Creating the wrt variable
    double complex saved_var;
    bool var_created = false;
    // if the variable exists, save off its current value
    if (!vars_get(var, &saved_var)) {
        
        // if the idiot user tried to use "ans", fail out
        if (strcmp(var, "ans") == 0) {
            return CALC_RETURN_VAR_GET_ERROR;
        }

        // if the variable does not exist, create it
        var_created = true;
        if (!vars_set(var, at_value)) {
            // if the idiot user tried to use a system variable name, fail out
            return CALC_RETURN_VAR_SET_ERROR;
        }
    }
    // from now on we know var is a valid variable name! No need to check its return

    // === Get Lower Value
    vars_set(var, at_value - NUMERICAL_H);
    funcs_clear_error();
    double complex f_minus;
    uint8_t depth = 0;
    calc_return_t ret_val = ast_eval(tree, &f_minus, &depth);
    if (!ast_return_check(&ret_val, &f_minus, result->data.str_out)) return ret_val;

    // === Get Upper Value
    vars_set(var, at_value + NUMERICAL_H);
    funcs_clear_error();
    depth = 0;
    ret_val = ast_eval(tree, &result->data.number, &depth);
    if (!ast_return_check(&ret_val, &result->data.number, result->data.str_out)) return ret_val;

    // === Central Difference Derivative Formula
    result->data.number = (result->data.number - f_minus) / (2.0 * NUMERICAL_H);

    // === Restore Original Variable value
    if (!var_created) {
        vars_set(var, saved_var);
    } else {
        vars_remove(var);
    }

    return CALC_RETURN_SUCCESS;
}

calc_return_t nsolve_root(ASTNode *tree, const char *var, double complex guess_val, calc_result_t *result) {

    // === Saving off or Creating the wrt variable
    double complex saved_var;
    bool var_created = false;
    // if the variable exists, save off its current value
    if (!vars_get(var, &saved_var)) {
        
        // if the idiot user tried to use "ans", fail out
        if (strcmp(var, "ans") == 0) {
            return CALC_RETURN_VAR_GET_ERROR;
        }

        var_created = true;
    }

    if (!vars_set(var, guess_val)) {
        // if the idiot user tried to use a system variable name, fail out
        return CALC_RETURN_VAR_SET_ERROR;
    }
    // from now on we know var is a valid variable name! No need to check its return

    uint8_t loop_count = 0;
    double complex f_val, f_prime, var_val;
    calc_return_t ret_val;
    bool solved = false;
    uint8_t depth = 0;

    funcs_clear_error();

    while (loop_count < ROOT_SOLVE_LOOP_MAX) {

        depth = 0;
        ret_val = ast_eval(tree, &f_val, &depth);
        if (!ast_return_check(&ret_val, &f_val, result->data.str_out)) return ret_val;

        if (cabs(f_val) < 1e-12) {
            solved = true;
            break;
        }

        ret_val = nsolve_derivative(tree, var, guess_val, result);
        if (!ast_return_check(&ret_val, &result->data.number, result->data.str_out)) return ret_val;
        f_prime = result->data.number;

        if (cabs(f_prime) < 1e-15) {
            if (!var_created) vars_set(var, saved_var);
            return CALC_NUM_SOLVE_DERIV_FAIL;
        }

        guess_val = guess_val - f_val / f_prime;
        vars_set(var, guess_val);

        loop_count += 1;
    }

    if (!var_created) { 
        vars_set(var, saved_var);
    } else {
        vars_remove(var);
    }

    if (solved) {
        result->data.number = guess_val;
        return CALC_RETURN_SUCCESS;

    } else {
        return CALC_NUM_SOLVE_LOOP_FAIL;
    }

}

calc_return_t nsolve_integrate(ASTNode *tree, const char *var, double lower, double upper, calc_result_t *result) {

    double complex saved_var;
    bool var_created = false;
    if (!vars_get(var, &saved_var)) {
        
        // if the idiot user tried to use "ans", fail out
        if (strcmp(var, "ans") == 0) {
            return CALC_RETURN_VAR_GET_ERROR;
        }

        // if the variable does not exist, create it
        var_created = true;
        if (!vars_set(var, 0.0)) { //dummy value
            // if the idiot user tried to use a system variable name, fail out
            return CALC_RETURN_VAR_SET_ERROR;
        }
    }
    // from now on we know var is a valid variable name! No need to check its return

    // WE store "h" in "upper" to save memory. In the real math algorithm, this is h
    upper = (upper - lower) / NUMERICAL_SUBINTERVALS; 
    double complex sum = 0;
    uint8_t depth;
    calc_return_t ret_val;

    for (uint16_t i = 0; i <= NUMERICAL_SUBINTERVALS; i++) {

        vars_set(var, lower + i * upper);
        double complex f_i;
        depth = 0;
        ret_val = ast_eval(tree, &f_i, &depth);
        if (!ast_return_check(&ret_val, &f_i, result->data.str_out)) {
            if (!var_created) vars_set(var, saved_var); // if we do fail some math, restore the var to its original value anyway
            return ret_val;
        }

        if (i == 0 || i == NUMERICAL_SUBINTERVALS) {
            sum += f_i;
            continue;
        }
        if (i & 1) { // ODD
            sum += 4.0 * f_i;
        } else { // EVEN
            sum += 2.0 * f_i;
        }
    }

    if (!var_created) {
        vars_set(var, saved_var);
    } else {
        vars_remove(var);
    }

    result->data.number = sum * upper / 3.0;
    return CALC_RETURN_SUCCESS;
}



static bool ast_return_check(calc_return_t *ret_val, double complex *result, char *str_out) {

    if (*ret_val != CALC_RETURN_SUCCESS) return false;

    if (isnan(creal(*result))) {
        if (funcs_get_error() != NULL) {
            memcpy(str_out, funcs_get_error(), CALC_ERR_MSG_LEN+1);
        } else {
            ERR_SEND(str_out, "GENERIC MATH ERROR");
        }
        *ret_val = CALC_RETURN_UNKNOWN_ERROR;
        return false;
    }

    if (isinf(creal(*result))) {
        *ret_val = CALC_INFINITY_RESULT;
        return false;
    }

    return true;
}
