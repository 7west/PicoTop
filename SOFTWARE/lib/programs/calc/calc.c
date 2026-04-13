#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "terminal.h"
#include "os_debug.h"
#include "shell.h"

#include "calc.h"
#include "math/vars.h"
#include "math/eval.h"
#include "calc_types.h"
#include "bg/calc_edit.h"
#include "bg/calc_ui.h"
#include "bg/calc_file.h"

#define CALC_INIT_CELL_NUM 5



typedef struct {
    calc_cell_t cells[CALC_NUM_CELLS];
    calc_cell_t *cells_ptrs[CALC_NUM_CELLS];
    calc_cell_t temp_cell; // holds currently editing cell
    calc_state_t state;
} Calc_Memory;


static Calc_Memory *cmem;

const prog_vtable_t calc_prog = {
    .name = "calc",
    .enter = calc_enter,
    .exit = calc_exit,
    .on_key = calc_on_key,
    .tick = calc_tick,
    .periodic = NULL,
};

static void calc_init_ui(void);
static void calc_local_init(void);

static bool calc_key_input_all_modes(key_event_t key);
static void calc_key_input_edit_mode(key_event_t key);
static void calc_key_input_nav_mode(key_event_t key);

static void calc_start_edit_mode(void);
static void calc_start_nav_mode(void);

static void load_temp_cell(void);
static void write_temp_cell(void);

static void calc_arrow_down(void);
static void calc_arrow_up(void);
static void calc_shift_cell_up(void);
static void calc_shift_cell_down(void);
static void calc_calculate_all_cells(void);
static void calc_add_cell(void);
static void calc_rem_cell(void);
static bool cell_was_modified(void);

static bool calc_valid_symbol(uint8_t keycode);





void calc_enter(void) {
    term_clear_prog_screen();

    uint8_t *prog_mem = (uint8_t *)prog_get_mem();
    memset(prog_mem, 0x00, PROG_MEM_SIZE);
    size_t offset = 0;

    vars_init(prog_mem + offset);
    offset += vars_mem_size();

    eval_init(prog_mem + offset);
    offset += eval_mem_size();

    cmem = (Calc_Memory *)(prog_mem + offset);
    calc_local_init();

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);

    calc_init_ui();
}

void calc_exit(void) {
    uint8_t *prog_mem = (uint8_t *)prog_get_mem();
    memset(prog_mem, 0x00, PROG_MEM_SIZE);
}

// pass keys to various modes's handlers
void calc_on_key(key_event_t key) {
    if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'x' || key.keycode == 'X')) {
        prog_switch(&shell_prog);
        return;
    }

    // in Help screen, do nothing except exit Help screen
    if (cmem->state.mode == CALC_HELP_MODE) {
        if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'h')) {
            term_clear_prog_screen();
            cmem->state.mode = CALC_NAV_MODE;
            calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
            calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
        }
        return;
    }
    
    if (calc_key_input_all_modes(key)) return;

    if (cmem->state.mode == CALC_NAV_MODE) {
        calc_key_input_nav_mode(key);
    } else if (cmem->state.mode == CALC_EDIT_MODE) {
        calc_key_input_edit_mode(key);
    }
}

void calc_tick(void) {
    
}






static void calc_init_ui(void) {

    // draw bottom UI:
    calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
}

static void calc_local_init(void) {
    cmem->state.cell_count = CALC_INIT_CELL_NUM;
    cmem->state.curr_cell = 0;
    cmem->state.cursor_pos = 0;
    cmem->state.top_ui_cell = 0;
    cmem->state.mode = CALC_NAV_MODE;
    cmem->state.rad_enabled = true;
    // because temp_cell will always be on a cell that exists
    cmem->temp_cell.flags = CALC_CELL_FLAG_EXISTS;

    for (uint8_t i = 0; i < CALC_INIT_CELL_NUM; i++) {
        cmem->cells[i].flags = CALC_CELL_FLAG_EXISTS;
    }

    // strings are filled with ' ' vice \0 in calc because terminal will stop printing at \0
    //  so this way we actually blank rows we removed chars from
    for (uint16_t i = 0; i < CALC_NUM_CELLS; i++) {
        memset(cmem->cells[i].expr, ' ', CALC_EXPR_TOT_LEN);
        memset(cmem->cells[i].out_msg, ' ', CALC_OUT_MSG_LEN);
        cmem->cells_ptrs[i] = &cmem->cells[i];
    }
}

// returns true if the key triggered an action here
static bool calc_key_input_all_modes(key_event_t key) {

    if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == (KBD_BASCII_ARROW_DOWN)) {
        calc_shift_cell_down();
        return true;

    } else if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == (KBD_BASCII_ARROW_UP)) {
        calc_shift_cell_up();
        return true;

    } else if (key.keycode == KBD_BASCII_ARROW_DOWN) {
        calc_arrow_down();
        return true;

    } else if (key.keycode == KBD_BASCII_ARROW_UP) {
        calc_arrow_up();
        return true;

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'a')) {
        calc_add_cell();
        return true;

    // toggle Radians vs Degree Mode
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'r')) {
        cmem->state.rad_enabled = !cmem->state.rad_enabled;
        calc_calculate_all_cells();
        calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
        if (cmem->state.mode == CALC_EDIT_MODE) {
            calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        }
        calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
        return true;

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'd')) {
        calc_rem_cell();
        return true;

        // Enter HELP Mode
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'h')) {
        
        term_clear_prog_screen();
        write_temp_cell();
        // so the temp_cell empty out_msg gets filled in
        if (cmem->state.mode == CALC_EDIT_MODE) calc_calculate_all_cells();

        cmem->state.mode = CALC_HELP_MODE;
        calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
        calc_draw_help_ui();
        return true;

    } else {
        return false;
    }
}

static void calc_key_input_edit_mode(key_event_t key) {

    // === Changing Modes:
    if (key.keycode == KBD_BASCII_NEW_LINE) {
        cmem->state.mode = CALC_NAV_MODE;
        calc_start_nav_mode();

    // === Cell Editing:
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'z')) {
        load_temp_cell();
        if (cmem->state.cursor_pos > cmem->temp_cell.len) cmem->state.cursor_pos = cmem->temp_cell.len;
        calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);

    }  else if (key.keycode == KBD_BASCII_BACKSPACE) {
        calc_edit_bksp(&cmem->temp_cell, &cmem->state.cursor_pos);
        calc_draw_one_cell(cmem->state.curr_cell - cmem->state.top_ui_cell, 
                                &cmem->temp_cell, cmem->state.curr_cell, true);
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        
    }  else if (key.keycode == KBD_BASCII_DELETE) {
        calc_edit_del(&cmem->temp_cell, &cmem->state.cursor_pos);
        calc_draw_one_cell(cmem->state.curr_cell - cmem->state.top_ui_cell, 
                                &cmem->temp_cell, cmem->state.curr_cell, true);
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        
    }  else if (calc_valid_symbol(key.keycode)) {
        calc_edit_insert(key.keycode, &cmem->temp_cell, &cmem->state.cursor_pos);
        calc_draw_one_cell(cmem->state.curr_cell - cmem->state.top_ui_cell, 
                                &cmem->temp_cell, cmem->state.curr_cell, true);
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);

    // === Cell Navigation:
    } else if (key.keycode == KBD_BASCII_ARROW_LEFT) {
        if (cmem->state.cursor_pos > 0) {
            cmem->state.cursor_pos -= 1;
            calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        }

    } else if (key.keycode == KBD_BASCII_ARROW_RIGHT) {
        // being edited so temp_cell holds the true length
        if (cmem->state.cursor_pos < cmem->temp_cell.len) {
            cmem->state.cursor_pos += 1;
            calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        }

    } else if (key.keycode == KBD_BASCII_END) {
        cmem->state.cursor_pos = cmem->temp_cell.len;
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);

    } else if (key.keycode == KBD_BASCII_HOME) {
        cmem->state.cursor_pos = 0;
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);

    }
}

static void calc_key_input_nav_mode(key_event_t key) {
    if (key.keycode == KBD_BASCII_NEW_LINE) {
        cmem->state.mode = CALC_EDIT_MODE;
        calc_start_edit_mode();

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 's')) {
        calc_save_file(cmem->cells, cmem->state);
        calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
        
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'o')) {
        calc_load_file(cmem->cells, &cmem->state);
        calc_wipe_main_ui();
        calc_calculate_all_cells();
        calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
        calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
    }
}

static void calc_start_edit_mode(void) {
    cmem->state.cursor_pos = 0;
    calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);

    load_temp_cell();

    calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
}

static void calc_start_nav_mode(void) {

    calc_remove_cursor();

    write_temp_cell();

    calc_calculate_all_cells();

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);

    calc_bottom_ui_update(cmem->state.mode, cmem->state.cell_count, cmem->state.rad_enabled);
}

// load cells[curr_cell] into temp_cell
static void load_temp_cell(void) {
    memcpy(&cmem->temp_cell, &cmem->cells[cmem->state.curr_cell], sizeof(calc_cell_t));
    memset(cmem->temp_cell.out_msg, ' ', CALC_OUT_MSG_LEN); // so the editing cell never shows a result or error
    cmem->cells_ptrs[cmem->state.curr_cell] = &cmem->temp_cell;
}

// write temp_cell to cells[curr_cell]
static void write_temp_cell(void) {
    // memcpy(&cmem->cells[cmem->state.curr_cell], &cmem->temp_cell, sizeof(calc_cell_t));

    memcpy(&cmem->cells[cmem->state.curr_cell].expr, &cmem->temp_cell.expr, CALC_EXPR_TOT_LEN+1);
    cmem->cells[cmem->state.curr_cell].flags = cmem->temp_cell.flags;
    cmem->cells[cmem->state.curr_cell].len = cmem->temp_cell.len;
    cmem->cells[cmem->state.curr_cell].result = cmem->temp_cell.result;


    cmem->cells_ptrs[cmem->state.curr_cell] = &cmem->cells[cmem->state.curr_cell];
}

static void calc_shift_cell_down(void) {
    if (cmem->state.mode != CALC_NAV_MODE) return;
    if (cmem->state.curr_cell == cmem->state.cell_count - 1) return;

    memcpy(&cmem->temp_cell, &cmem->cells[cmem->state.curr_cell], sizeof(calc_cell_t));
    memcpy(&cmem->cells[cmem->state.curr_cell], &cmem->cells[cmem->state.curr_cell + 1], sizeof(calc_cell_t));
    memcpy(&cmem->cells[cmem->state.curr_cell + 1], &cmem->temp_cell, sizeof(calc_cell_t));

    cmem->state.curr_cell += 1;
    calc_calculate_all_cells();

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);  
}

static void calc_shift_cell_up(void) {
    if (cmem->state.mode != CALC_NAV_MODE) return;
    if (cmem->state.curr_cell == 0) return;

    memcpy(&cmem->temp_cell, &cmem->cells[cmem->state.curr_cell], sizeof(calc_cell_t));
    memcpy(&cmem->cells[cmem->state.curr_cell], &cmem->cells[cmem->state.curr_cell - 1], sizeof(calc_cell_t));
    memcpy(&cmem->cells[cmem->state.curr_cell - 1], &cmem->temp_cell, sizeof(calc_cell_t));

    cmem->state.curr_cell -= 1;
    calc_calculate_all_cells();

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
    
}

static void calc_arrow_down(void) {
    if (cmem->state.curr_cell == cmem->state.cell_count-1) return;

    if (cmem->state.mode == CALC_EDIT_MODE) write_temp_cell();
    cmem->state.curr_cell += 1;
    if (cmem->state.curr_cell >= cmem->state.top_ui_cell + CALC_CELLS_PER_WINDOW) {
        cmem->state.top_ui_cell += 1;
    }

    if (cmem->state.mode == CALC_EDIT_MODE) {
        
        if (cell_was_modified()) calc_calculate_all_cells();
        load_temp_cell();

        cmem->state.cursor_pos = 0;
    }

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);

    if (cmem->state.mode == CALC_EDIT_MODE) {        
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
    }
}

static void calc_arrow_up(void) {
    if (cmem->state.curr_cell == 0) return;

    if (cmem->state.mode == CALC_EDIT_MODE) write_temp_cell();
    cmem->state.curr_cell -= 1;
    if (cmem->state.curr_cell < cmem->state.top_ui_cell) {
        cmem->state.top_ui_cell -= 1;
    }

    if (cmem->state.mode == CALC_EDIT_MODE) {
        
        if (cell_was_modified()) calc_calculate_all_cells();
        load_temp_cell();

        cmem->state.cursor_pos = 0;
    }

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);

    if (cmem->state.mode == CALC_EDIT_MODE) {        
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
    }
}

static bool cell_was_modified(void) {
    bool ret_val = false;
    for (uint16_t i = 0; i < cmem->state.cell_count; i++) {
        if (cmem->cells[i].flags & CALC_CELL_FLAG_MODIFIED) {
            ret_val = true;
        }
    }
    return ret_val;
}

static void calc_calculate_all_cells(void) {

    calc_eval_reset_ans();

    for (uint16_t i = 0; i < cmem->state.cell_count; i++) {
        // cell is no longer recently modified because we are calculating all cells
        cmem->cells[i].flags &= ~(CALC_CELL_FLAG_MODIFIED);

        // skip empty cells
        if (!(cmem->cells[i].flags & CALC_CELL_FLAG_OCCUPIED)) continue;

        if (calc_eval(cmem->cells[i].expr, &cmem->cells[i].result, cmem->cells[i].out_msg, cmem->state.rad_enabled)) {
            cmem->cells[i].flags |= CALC_CELL_FLAG_VALID;

            calc_format_double(cmem->cells[i].result, cmem->cells[i].out_msg);
            // snprintf(cmem->cells[i].out_msg, CALC_ERR_MSG_LEN, "%#.10g", cmem->cells[i].result);

        } else {
            cmem->cells[i].flags &= ~(CALC_CELL_FLAG_VALID);
        }
    }
}

// for user entering chars
static bool calc_valid_symbol(uint8_t keycode) {
    if (keycode >= ' ' && keycode <= '~') return true;
    return false;
}

static void calc_rem_cell(void) {
    if (cmem->state.cell_count == 1) return;

    if (cmem->state.mode == CALC_EDIT_MODE) {
        write_temp_cell();
    }

    memmove(&cmem->cells[cmem->state.curr_cell],
        &cmem->cells[cmem->state.curr_cell + 1],
        (cmem->state.cell_count - cmem->state.curr_cell - 1) * sizeof(calc_cell_t));

    memset(&cmem->cells[cmem->state.cell_count - 1], 0x00, sizeof(calc_cell_t));
    memset(cmem->cells[cmem->state.cell_count - 1].expr, ' ', CALC_EXPR_TOT_LEN);
    memset(cmem->cells[cmem->state.cell_count - 1].out_msg, ' ', CALC_OUT_MSG_LEN);
        
    cmem->state.cell_count -= 1;
    if (cmem->state.curr_cell > cmem->state.cell_count-1) {
        cmem->state.curr_cell = cmem->state.cell_count-1;
    }
    
    if (cmem->state.mode == CALC_EDIT_MODE) {
        load_temp_cell();
    }

    calc_calculate_all_cells();

    // wipes the UI elements if the last cell is visible on the screen
    //  if not, UI elements stay for moved cells
    if (cmem->state.top_ui_cell + CALC_CELLS_PER_WINDOW > cmem->state.cell_count) {
        calc_wipe_ui_symbols();
    }
    

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);

    if (cmem->state.mode == CALC_EDIT_MODE) {
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
        calc_update_cell_count(cmem->state.cell_count);
    }
}

// does not trigger recalculation because adding a cell does not mess with ans
//  blank cells just carry the ans
static void calc_add_cell(void) {
    if (cmem->state.cell_count == CALC_NUM_CELLS) return;
  
    memmove(&cmem->cells[cmem->state.curr_cell + 2],
            &cmem->cells[cmem->state.curr_cell + 1],
            (cmem->state.cell_count - cmem->state.curr_cell - 1) * sizeof(calc_cell_t));
    
    memset(&cmem->cells[cmem->state.curr_cell + 1], 0x00, sizeof(calc_cell_t));

    cmem->cells[cmem->state.curr_cell + 1].flags = CALC_CELL_FLAG_EXISTS;
    memset(cmem->cells[cmem->state.curr_cell + 1].expr, ' ', CALC_EXPR_TOT_LEN);
    memset(cmem->cells[cmem->state.curr_cell + 1].out_msg, ' ', CALC_OUT_MSG_LEN);

    cmem->state.cell_count += 1;

    calc_draw_all_cells(cmem->cells_ptrs, cmem->state.top_ui_cell, cmem->state.curr_cell);
    calc_update_cell_count(cmem->state.cell_count);
    if (cmem->state.mode == CALC_EDIT_MODE) {
        calc_draw_cursor(cmem->state.top_ui_cell, cmem->state.curr_cell, cmem->state.cursor_pos);
    }
}


bool calc_str_eval(const char *input, double *result, char *out_msg) {

    uint8_t *prog_mem = (uint8_t *)prog_get_mem();
    size_t offset = 0;

    vars_init(prog_mem + offset);
    offset += vars_mem_size();

    eval_init(prog_mem + offset);
    offset += eval_mem_size();

    cmem = (Calc_Memory *)(prog_mem + offset);

    if (!calc_eval(input, result, out_msg, true)) {
        printf("error: %s\n", out_msg);
    } else {
        calc_format_double(*result, out_msg);
    }

    memset(prog_mem, 0x00, PROG_MEM_SIZE);

    return true;
}

