#pragma once

#include "prog.h"

extern const prog_vtable_t hangman_prog;








void hangman_enter(void);

void hangman_exit(void);

void hangman_on_key(key_event_t key);