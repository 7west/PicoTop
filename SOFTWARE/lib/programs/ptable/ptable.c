#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"

#include "ptable.h"
#include "terminal.h"
#include "shell.h"
#include "elements.h"
#include "ptable_help.h"


#define VERT_LINE_SYM 179 // │ 
#define RIGHT_ANGLE_SYM 218 // ┌
#define HORIZ_LINE_SYM 196 // ─
#define DOWN_ARROW_SYM 25 // ↓
#define ENTER_RETURN_SYM 28 //Enter/Return/New Line symbol


typedef struct {
    uint8_t selected_element;
    char snprintf_row[TERM_NUM_COLS + 1];
    bool help_mode;
    bool detail_mode;
} PTable_Memory;

static_assert(sizeof(PTable_Memory) <= PROG_MEM_SIZE, "PTable_Memory exceeds the 100kB shared memory bank!");

static PTable_Memory *mem;

const prog_vtable_t ptable_prog = {
    .name = "Periodic Table",
    .enter = ptable_enter,
    .exit = ptable_exit,
    .on_key = ptable_on_key,
    .tick = NULL,
    .periodic = NULL,
};

static void draw_full_ptable(void);
static void draw_one_element(uint8_t atomic_number);
static void draw_table_markers(void);
static uint8_t nav_step(uint8_t atomic_num, int8_t delta_col, int8_t delta_row);
static void invert_element(uint8_t atomic_num);
static void clear_selected_element(void);
static void select_element(uint8_t atomic_num);
static void ptable_draw_help_ui(void);
static void ptable_draw_detail_ui(void);



void ptable_enter(void) {
    term_clear_prog_screen();

    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);
    mem = (PTable_Memory *)prog_get_mem();

    draw_full_ptable();
    draw_table_markers();

    select_element(1);
}

void ptable_exit(void) {
    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);
    term_clear_prog_screen();
}

void ptable_on_key(key_event_t key) {

    if (mem->help_mode) {
        if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'h' || key.keycode == 'H')) {
            // exit help mode
            mem->help_mode = false;
            term_clear_prog_screen();
            draw_full_ptable();
            draw_table_markers();
            select_element(mem->selected_element);
            return;

        } else {
            return;
        }
    }
    if (mem->detail_mode) {
        if (key.keycode == KBD_BASCII_NEW_LINE) {
            // exit detail mode
            mem->detail_mode = false;
            term_clear_prog_screen();
            draw_full_ptable();
            draw_table_markers();
            select_element(mem->selected_element);
            return;

        } else {
            return;
        }
    }

    if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'x' || key.keycode == 'X')) {
        prog_switch(&shell_prog);
        return;

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'h' || key.keycode == 'H')) {
        term_clear_prog_screen();
        mem->help_mode = true;
        ptable_draw_help_ui();
        return;

    } else if (key.keycode == KBD_BASCII_NEW_LINE) {
        // open element details
        term_clear_prog_screen();
        mem->detail_mode = true;
        ptable_draw_detail_ui();
        return;

    } else if (key.keycode == KBD_BASCII_ARROW_UP) {
        clear_selected_element();
        select_element(nav_step(mem->selected_element, 0, -1));
        return;

    } else if (key.keycode == KBD_BASCII_ARROW_DOWN) {
        clear_selected_element();
        select_element(nav_step(mem->selected_element, 0, 1));
        return;

    } else if (key.keycode == KBD_BASCII_ARROW_LEFT) {
        clear_selected_element();
        select_element(nav_step(mem->selected_element, -1, 0));
        return;

    } else if (key.keycode == KBD_BASCII_ARROW_RIGHT) {
        clear_selected_element();
        select_element(nav_step(mem->selected_element, 1, 0));
        return;

    }
}

static void clear_selected_element(void) {
    invert_element(mem->selected_element);
    term_clear_line(TERM_NUM_ROWS-1);
}

static void select_element(uint8_t atomic_num) {
    mem->selected_element = atomic_num;
    invert_element(atomic_num);

    term_draw_string(1, TERM_NUM_ROWS-1, elements[atomic_num - 1].name);
    snprintf(mem->snprintf_row, TERM_NUM_COLS+1, "%.5f", elements[atomic_num - 1].atomic_weight);
    term_draw_string(16, TERM_NUM_ROWS-1, mem->snprintf_row);
    term_draw_string(30, TERM_NUM_ROWS-1, elements[atomic_num - 1].electron_config);

    term_draw_string(62, TERM_NUM_ROWS-1, "^X Exit | ^H Help");

    term_invert_line(TERM_NUM_ROWS-1);
}

static void invert_element(uint8_t atomic_num) {
    uint8_t x = element_cell[atomic_num - 1].x_cell + 1;
    uint8_t y = element_cell[atomic_num - 1].y_cell + 1;

    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 2; j++) {

            term_invert_cell(x + i, y + j);
        }
    }
}

static uint8_t nav_step(uint8_t atomic_num, int8_t delta_col, int8_t delta_row) {
    printf("start num: %u\n", atomic_num);
    int32_t col = (element_cell[atomic_num - 1].x_cell - 2) / 4;
    int32_t row = (element_cell[atomic_num - 1].y_cell - 2) / 3;

    if (delta_row == 1 && delta_col == 0 && row == 6 && nav_grid[7][col] == 0) {
        if (atomic_num < 89) return 57;
        return 70;
    }
    
    col += delta_col;
    row += delta_row;

    // no vertical wrap
    if (row < 0 || row >= PT_NAV_ROWS) return atomic_num;

    // wrap horizontally
    if (col < 0) {
        col = PT_NAV_COLS - 1;
    } else if (col >= PT_NAV_COLS) {
        col = 0;
    }

    while (nav_grid[row][col] == 0) {

        col += delta_col;
        // wrap horizontally
        if (col < 0) {
            col = PT_NAV_COLS - 1;
        } else if (col >= PT_NAV_COLS) {
            col = 0;
        }

        // normal move up or down
        if (delta_col == 0) {
            row += delta_row;
            if (row < 0 || row >= PT_NAV_ROWS) return atomic_num;
        }
    }

    return nav_grid[row][col];
}


static void draw_full_ptable(void) {

    for (uint8_t i = 1; i <= NUM_ELEMENTS; i++) {
        draw_one_element(i);
    }

}

static void draw_one_element(uint8_t atomic_num) {
    uint8_t x = element_cell[atomic_num - 1].x_cell;
    uint8_t y = element_cell[atomic_num - 1].y_cell;

    term_draw_char(x, y, RIGHT_ANGLE_SYM);
    term_draw_char(x + 1, y, HORIZ_LINE_SYM);
    term_draw_char(x + 2, y, HORIZ_LINE_SYM);
    term_draw_char(x + 3, y, HORIZ_LINE_SYM);
    term_draw_char(x, y + 1, VERT_LINE_SYM);
    term_draw_char(x, y + 2, VERT_LINE_SYM);

    char atomic_num_str[4] = {0};
    snprintf(atomic_num_str, 4, "%u", atomic_num);
    term_draw_string(x + 1, y + 1, atomic_num_str);
    term_draw_string(x + 1, y + 2, elements[atomic_num - 1].symbol);
}

static void ptable_draw_help_ui(void) {
    for (uint8_t i = 0; i < TERM_NUM_ROWS - 2; i++) {
        term_draw_string(0, i + 1, PTABLE_HELP[i]);
    }
    term_draw_string(1, TERM_NUM_ROWS-1, "^H to exit Help");
    term_invert_line(TERM_NUM_ROWS-1);
}

static void ptable_draw_detail_ui(void) {
    
    term_draw_string(1, 2, elements[mem->selected_element - 1].name);
    term_draw_string(1, 3, "===============================");

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Atomic Number: %u", 
        elements[mem->selected_element - 1].atomic_number);
    term_draw_string(1, 4, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Period: %u", 
        elements[mem->selected_element - 1].period);
    term_draw_string(1, 5, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Group: %u", 
        elements[mem->selected_element - 1].group);
    term_draw_string(1, 6, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Block: %c", 
        elements[mem->selected_element - 1].block);
    term_draw_string(1, 7, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Category: %s", 
        elements[mem->selected_element - 1].category);
    term_draw_string(1, 8, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Electron Config: %s", 
        elements[mem->selected_element - 1].electron_config);
    term_draw_string(1, 9, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Atomic Weight: %0.5f", 
        elements[mem->selected_element - 1].atomic_weight);
    term_draw_string(1, 10, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Density @STP: %0.5f g/cm3", 
        elements[mem->selected_element - 1].atomic_number);
    term_draw_string(1, 11, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Melting Point: %0.5f K", 
        elements[mem->selected_element - 1].melting_point_K);
    term_draw_string(1, 12, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Boiling Point: %0.5f K", 
        elements[mem->selected_element - 1].boiling_point_K);
    term_draw_string(1, 13, mem->snprintf_row);

    snprintf(mem->snprintf_row, TERM_NUM_COLS + 1, "Electronegativity: %0.3f", 
        elements[mem->selected_element - 1].electronegativity);
    term_draw_string(1, 14, mem->snprintf_row);

    term_draw_string(1, TERM_NUM_ROWS-1, "  to exit Details");
    term_draw_char(1, TERM_NUM_ROWS-1, ENTER_RETURN_SYM);
    term_invert_line(TERM_NUM_ROWS-1);
}

static void draw_table_markers(void) {
    
    // draw groups
    term_draw_string(3, 1, "s1");
    term_draw_string(7, 1, "s2");
    term_draw_string(15, 1, "d3");
    term_draw_string(19, 1, "d4");
    term_draw_string(23, 1, "d5");
    term_draw_string(27, 1, "d6");
    term_draw_string(31, 1, "d7");
    term_draw_string(35, 1, "d8");
    term_draw_string(39, 1, "d9");
    term_draw_string(43, 1, "d10");
    term_draw_string(47, 1, "d11");
    term_draw_string(51, 1, "d12");
    term_draw_string(55, 1, "p13");
    term_draw_string(59, 1, "p14");
    term_draw_string(63, 1, "p15");
    term_draw_string(67, 1, "p16");
    term_draw_string(71, 1, "p17");
    term_draw_string(75, 1, "p18");

    // draw periods
    term_draw_string(0, 3, "1");
    term_draw_string(0, 6, "2");
    term_draw_string(0, 9, "3");
    term_draw_string(0, 12, "4");
    term_draw_string(0, 15, "5");
    term_draw_string(0, 18, "6");
    term_draw_string(0, 21, "7");
    term_draw_string(0, 24, "6");
    term_draw_string(0, 27, "7");

    // f-block indicator
    term_draw_char(11, 22, DOWN_ARROW_SYM);
    term_draw_char(12, 22, 'f');
    term_draw_char(13, 22, DOWN_ARROW_SYM);

}