#include <stdio.h>
#include "pico/stdlib.h"
#include <time.h>

#include "prog.h"
#include "terminal.h"
#include "rtc.h"

static uint8_t shared_prog_memory[PROG_MEM_SIZE];

static const prog_vtable_t *active_prog;

void prog_switch(const prog_vtable_t *next_prog) {
    if (active_prog && active_prog->exit) active_prog->exit();
    active_prog = next_prog;

    draw_ui_bar();

    if (active_prog && active_prog->enter) {
        active_prog->enter();
    }

}

void *prog_get_mem(void) {
    return shared_prog_memory;
}

const prog_vtable_t* prog_get_active(void) {
    return active_prog;
}

void draw_ui_bar(void) {
    term_clear_line(0);
    term_draw_string(1, 0, prog_get_active()->name);

    term_invert_line(0); // time handles its own color inversion

    update_ui_time();
}

// draws date and time in the top right of UI
//  draws and inverts itself
void update_ui_time(void) {

    char time_string[17] = {0};
    rtc_get_time_string(time_string);

    for (uint8_t i = 0; i < 16; i++) {
        term_draw_char(64 + i, 0, time_string[i]);
        term_invert_cell(64 + i, 0);
    }
}