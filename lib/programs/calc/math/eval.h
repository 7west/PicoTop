#pragma once

















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
bool calc_eval(const char *input, double *result, char *err_msg, bool rad_mode);

// for when you recalculate all cells. ans must be 0 (that is just the original state we decided on)
void calc_eval_reset_ans(void);

