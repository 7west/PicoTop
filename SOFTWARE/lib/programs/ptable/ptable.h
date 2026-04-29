#pragma once

#include "prog.h"

extern const prog_vtable_t ptable_prog;


void ptable_enter(void);

void ptable_exit(void);

void ptable_on_key(key_event_t key);