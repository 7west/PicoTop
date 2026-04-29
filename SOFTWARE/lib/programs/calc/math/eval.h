#pragma once

#include "calc_types.h"
// #include "ast.h"

typedef struct ASTNode ASTNode; 








size_t eval_mem_size(void);

// initializes memory and structs needed by eval
void eval_init(void *mem);


/**
 * @brief Performs evaluation of a cell by being passed the input string and outputs 
 * a double or error msg
 * 
 * @param input input string of chars
 * @param result correct result
 * @param err_msg limited to 23+1 char output
 * @param rad_mode passed in to set trig mode
 * @return false - calculation failed for some reason, check err_msg
 */
bool calc_eval(const char *input, calc_result_t *calc_result, bool rad_mode);

// for when you recalculate all cells. ans must be 0 (that is just the original state we decided on)
//  and all vars need to be undefined (not hold over old values)
void calc_eval_reset_vars(void);

/**
 * @brief Will solve an AST into a double result
 * 
 * @param node points to root of AST
 * @param result the actual number answer
 * @param depth MUST BE declared outside
 * @return calc_return_t - see 
 */
calc_return_t ast_eval(ASTNode *node, double complex *result, uint8_t *depth);