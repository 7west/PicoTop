#pragma once

#include "usb_kbd.h"

#define PROG_UI_START_COL 30
#define PROG_TOP_UI_BAR_ROW 0

#define PROG_MEM_SIZE (1024 * 100) // 102,400 bytes

typedef struct prog_vtable {
    const char *name;
    void (*enter)(void);
    void (*exit)(void);
    void (*on_key)(key_event_t key);
    void (*tick)(void);
    void (*periodic)(void);
} prog_vtable_t;

void prog_switch(const prog_vtable_t *next_prog);

void *prog_get_mem(void);

const prog_vtable_t* prog_get_active(void);

void draw_ui_bar(void);

void update_ui_time(void);