#pragma once

#include "prog.h"



extern const prog_vtable_t calc_prog;

void calc_enter(void);

void calc_exit(void);

void calc_on_key(key_event_t key);

void calc_tick(void);

// called by shell command to do a one-line math evaluation
bool calc_str_eval(const char *input, double *result, char *out_msg);


