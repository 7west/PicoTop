#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"

#include "uedit.h"
#include "vfs.h"
#include "os_debug.h"
#include "terminal.h"
#include "shell.h"
#include "uedit_doc.h"
#include "uedit_view.h"


#define UEDIT_MAX_PATH_LEN SHELL_CMD_LENGTH
#define UEDIT_TEMP_BUF_SIZE 256

// for huge memory uses in uEdit program
typedef struct {
    uedit_core_t core;

    uedit_doc_t doc_mem;
    uedit_view_t view_mem;

    char file_path[UEDIT_MAX_PATH_LEN+1];
    char temp_path[UEDIT_MAX_PATH_LEN+1];
    uint8_t temp_buf[256];
    char uedit_file_name[13];

} uEdit_Memory;

static_assert(sizeof(uEdit_Memory) <= PROG_MEM_SIZE, "uEdit_Memory exceeds the 100kB shared memory bank!");

static uEdit_Memory *mem;

file_open_mode_t file_flags;
static bool mod_star = false;
static bool help_active = false;
static bool stats_active = false;
static bool save_prompt_active = false;

const prog_vtable_t uedit_prog = {
    .name = "uEdit",
    .enter = uedit_enter,
    .exit = uedit_exit,
    .on_key = uedit_on_key,
    .tick = uedit_tick,
    .periodic = NULL,
};





void uedit_enter(void) {
    term_clear_prog_screen();

    _uedit_draw_init_ui();

    mem->core.cursor = 0;
    mem->core.vp_top_doc = 0;
    mod_star = false;

    help_active = false;
    stats_active = false;
    save_prompt_active = false;

    vp_init(&mem->core, &mem->view_mem);
    doc_init(&mem->core, &mem->doc_mem);

    vp_calc_all_rows();

    vp_render_rows();
    vp_render_vcursor();

    vp_update_cursor_corner();
}

void uedit_exit(void) {

    vfs_close(mem->core.fd);
    memset(mem, 0x00, sizeof(uEdit_Memory));

}

void uedit_on_key(key_event_t key) {

    if (help_active) {
        _uedit_help_input(key);
    } else if (stats_active) {
        _uedit_stats_input(key);
    } else if (save_prompt_active) {
        _uedit_save_input(key);
    } else {
        _uedit_input_layer(key);
    }

}

void uedit_tick(void) {
    
}




bool uedit_pass_file(const char *file, bool ro, bool force, const out_t *out) {

    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);

    mem = (uEdit_Memory *)prog_get_mem();

    // memset(&mem->core, 0x00, sizeof(uedit_core_t));
    // memset(uedit_file_name, 0x00, 13);
    mem->core.read_only = ro;

    if (vfs_build_valid_path(file, mem->file_path, true) < 0) {
        out->write(out->ctx, "Invalid File");
        return false;
    }

    file_flags = FS_MODE_WRITE | FS_MODE_CREATE;
    if (mem->core.read_only) file_flags = FS_MODE_READ_ONLY;
    if (force) file_flags |= FS_MODE_FORCE;

    mem->core.fd = vfs_open(file, file_flags);

    if (mem->core.fd <= 0) {
        
        if (mem->core.fd == 0) {
            out->write(out->ctx, "File is already open, Read Only, or System file. Adjust uedit options to open");

        } else if (mem->core.fd == -1) {
            out->write(out->ctx, "Invalid File");
            mem->core.fd = 0;
        }
        return false;
    }

    mem->core.file_size = vfs_get_file_size(mem->core.fd);

    return true;
}



static bool _uedit_save_file(void) {

    // store timestamp for "Saving..." text
    uint64_t start_time = time_us_64();

    // === Draw bottom UI bar
    term_clear_line(TERM_NUM_ROWS-1);
    term_draw_string(1, TERM_NUM_ROWS-1, "Saving.");
    term_invert_line(TERM_NUM_ROWS-1);
    uint8_t dot_idx = 8;

    // === create temp file path
    memcpy(mem->temp_path, mem->file_path, UEDIT_MAX_PATH_LEN + 1);

    uint16_t path_len = strlen(mem->temp_path);
    uint16_t path_rev_idx = path_len - 1;
    while (path_rev_idx > 0) { // a text file will NEVER occupy char 0 of file path

        if (mem->temp_path[path_rev_idx] == '.') { // found where file extension starts
            path_rev_idx += 1;
            break;
        }

        if (mem->temp_path[path_rev_idx] == '/') { // no '.' found, hit first '/'
            path_rev_idx = path_len; // end of path was end of file name w/o extension
            
            if (path_rev_idx < UEDIT_MAX_PATH_LEN) { // add the dot after checking path length
                mem->temp_path[path_rev_idx] = '.';
                path_rev_idx += 1;
            } else {
                debug_deposit("UEDT-CRIT-could not create temp file, file path > 256 chars. path_len:", path_len, DBG_U32_DEC);
                return false;
            }

        }
        path_rev_idx -= 1;
    }

    if (path_rev_idx + 2 >= UEDIT_MAX_PATH_LEN) {
        debug_deposit("UEDT-CRIT-could not create temp file, file path > 256 chars. path_len:", path_len, DBG_U32_DEC);
        return false;
    }

    mem->temp_path[path_rev_idx] = 'T';
    mem->temp_path[path_rev_idx + 1] = 'M';
    mem->temp_path[path_rev_idx + 2] = 'P';

    term_draw_char_inv(dot_idx++, TERM_NUM_ROWS-1, '.');

    // === Open temp file
    file_desc_t tfd = vfs_open(mem->temp_path, FS_MODE_WRITE | FS_MODE_CREATE);
    if (tfd < 0) {
        debug_deposit("UEDT-CRIT-could not create temp file, vfs_open failed", 0, DBG_NULL_VAR);
        return false;
    }
    
    term_draw_char_inv(dot_idx++, TERM_NUM_ROWS-1, '.');

    // === stream bytes from Piece Table to temp file
    uint32_t file_offset = 0;

    while (file_offset < mem->core.file_size) {

        int32_t write_size = (mem->core.file_size - file_offset > UEDIT_TEMP_BUF_SIZE) ?
            UEDIT_TEMP_BUF_SIZE : (mem->core.file_size - file_offset);


        if (!doc_read_file(file_offset, mem->temp_buf, write_size)) {
            debug_deposit("UEDT-CRIT-Reading Piece Table failed for temp file filling. offset:", file_offset, DBG_U32_DEC);
            return false;
        }

        if (vfs_write(tfd, mem->temp_buf, write_size) != write_size) {
            debug_deposit("UEDT-CRIT-Streaming to temp file failed at vfs_write(). offset:", file_offset, DBG_U32_DEC);
            return false;
        }

        file_offset += write_size;
    }
    term_draw_char_inv(dot_idx++, TERM_NUM_ROWS-1, '.');
    
    // === close temp file and original file
    vfs_close(tfd);
    // technically we don't need to close the original file to vfs_move()
    //      but is feels safer this way
    vfs_close(mem->core.fd); 

    term_draw_char_inv(dot_idx++, TERM_NUM_ROWS-1, '.');

    // === move temp file to overwrite original file
    bool forced = file_flags & FS_MODE_FORCE;
    vfs_move(mem->temp_path, mem->file_path, forced);

    term_draw_char_inv(dot_idx++, TERM_NUM_ROWS-1, '.');

    // === Repopulate Statics for new file
    // cursor, file_size, file_path, and vp_top_doc should not change
    mem->core.fd = vfs_open(mem->file_path, file_flags);

    if (mem->core.fd < 0) {
        mem->core.fd = 0;
        return false;
    }

    term_draw_char_inv(13, TERM_NUM_ROWS-1, '.');

    // reset doc model piece table
    doc_reset_ptable();

    // force "saving..." to stay on screen for at least 500ms
    //      TODO: eventually have text saying "Saved" appear somewhere
    while (time_us_64() < (start_time + 500000)) {
        // do nothing
    }

    // Update UI
    _uedit_clear_mod_star();
    _uedit_draw_bottom_ui();

    return true;
}






void _uedit_input_layer(key_event_t key) {

    if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'x' || key.keycode == 'X')) {
        if (!mod_star) {
            prog_switch(&shell_prog);
        } else {
            save_prompt_active = true;
            _uedit_draw_save_prompt();
        }
        
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 's' || key.keycode == 'S')) {
        if (mod_star && !mem->core.read_only && !_uedit_save_file()) {
            debug_deposit("UEDT-CRIT-File save failed!", 0, DBG_NULL_VAR);
        }
        
    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == KBD_BASCII_HOME)) {
        doc_cursor_top();
        vp_update_cursor_corner();

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == KBD_BASCII_END)) {
        vp_cursor_bottom();
        vp_update_cursor_corner();

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == KBD_BASCII_ARROW_LEFT)) {
        doc_cursor_start_word();
        vp_update_cursor_corner();

    } else if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == KBD_BASCII_ARROW_RIGHT)) {
        doc_cursor_end_word();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_ARROW_LEFT) {
        doc_cursor_decrease(); 
        vp_update_cursor_corner(); 

    } else if (key.keycode == KBD_BASCII_ARROW_RIGHT) {
        doc_cursor_increase();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_ARROW_DOWN) {
        vp_cursor_down();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_ARROW_UP) {
        vp_cursor_up();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_HOME) {
        vp_cursor_home();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_END) {
        vp_cursor_end();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_PAGE_DOWN) {
        vp_cursor_pgdn();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_PAGE_UP) {
        vp_cursor_pgup();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_BACKSPACE && !mem->core.read_only) {
        if (doc_bksp_char()) _uedit_draw_mod_star();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_DELETE && !mem->core.read_only) {
        if (doc_del_char()) _uedit_draw_mod_star();
        vp_update_cursor_corner();

    } else if (key.keycode == KBD_BASCII_F11) {
        doc_stream_file_UART();

    } else if (key.keycode == KBD_BASCII_F1) {
        help_active = true;
        vp_draw_help();

    } else if (key.keycode == KBD_BASCII_F2) {
        stats_active = true;
        vp_draw_stats();

    } else if (key.keycode == KBD_BASCII_F3 && !mem->core.read_only) {
        doc_set_copy_start();

    } else if (key.keycode == KBD_BASCII_F4 && !mem->core.read_only) {
        doc_set_copy_end();

    } else if (key.keycode == KBD_BASCII_F5 && !mem->core.read_only) {
        if(doc_paste()) _uedit_draw_mod_star();

    } else {
        // TODO: filter out special chars not used (F1-F12, Esc, Ins, etc)
        if (!mem->core.read_only) {
            if (doc_add_char(key.keycode)) _uedit_draw_mod_star();
            vp_update_cursor_corner();
        }
    
    }

}

static void _uedit_help_input(key_event_t key) {

    if (key.keycode == KBD_BASCII_F1) {
        help_active = false;

        vp_render_rows();
        vp_render_vcursor();

        _uedit_draw_bottom_ui();
    }
    
}

static void _uedit_stats_input(key_event_t key) {
    
    if (key.keycode == KBD_BASCII_F2) {
        stats_active = false;

        vp_render_rows();
        vp_render_vcursor();

        _uedit_draw_bottom_ui();
    }
}

static void _uedit_save_input(key_event_t key) {
    if (key.keycode == '\n') {

        if (mem->temp_buf[0] == 'y' || mem->temp_buf[0] == 'Y') {
            if (!_uedit_save_file()) {
                // if the save fails, don't exit
                //      there isn't anything the user can really do here, but it would be nice
                //      to let them have one last look at the changes they made that will be lost
                debug_deposit("UEDT-CRIT-File save failed!", 0, DBG_NULL_VAR);
                save_prompt_active = false;
                _uedit_draw_bottom_ui();
                return;
            }
        }
        prog_switch(&shell_prog);

    } else {
        mem->temp_buf[0] = key.keycode;
        term_draw_char_inv(42, TERM_NUM_ROWS-1, key.keycode);
    }
}





static void _uedit_draw_bottom_ui(void) {
    term_clear_line(TERM_NUM_ROWS-1);
    term_draw_string(1, TERM_NUM_ROWS-1, "^S Save | ^X Exit | F1 Help | F2 Stats     C");
    term_invert_line(TERM_NUM_ROWS-1);
}

static void _uedit_draw_init_ui(void) {
    char * file_name = vfs_get_file_name(mem->core.fd);

    // === store file name in nice form for uEdit uses
    uint8_t idx = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (file_name[i] != ' ') {
            mem->uedit_file_name[idx++] = file_name[i];
        }
    }
    mem->uedit_file_name[idx++] = '.';
    for (uint8_t i = 8; i < 11; i++) {
        if (file_name[i] != ' ') {
            mem->uedit_file_name[idx++] = file_name[i];
        }
    }

    // === Draw file name in top UI bar
    term_invert_line(PROG_TOP_UI_BAR_ROW);
    term_draw_string(PROG_UI_START_COL+1, PROG_TOP_UI_BAR_ROW, mem->uedit_file_name);
    if (mem->core.read_only) {
        term_draw_string(PROG_UI_START_COL+1 + strlen(mem->uedit_file_name), PROG_TOP_UI_BAR_ROW, " READ ONLY!");
        term_draw_char(PROG_UI_START_COL, PROG_TOP_UI_BAR_ROW, '!');
    }
    term_invert_line(PROG_TOP_UI_BAR_ROW);

    // === Draw bottom UI bar
    _uedit_draw_bottom_ui();
}

static void _uedit_draw_mod_star(void) {

    if (!mod_star) {
        term_draw_char(PROG_UI_START_COL, PROG_TOP_UI_BAR_ROW, '*');
        term_invert_cell(PROG_UI_START_COL, PROG_TOP_UI_BAR_ROW);
        mod_star = true;
    }
    
}

static void _uedit_clear_mod_star(void) {

    if (mod_star) {
        term_draw_char(PROG_UI_START_COL, PROG_TOP_UI_BAR_ROW, ' ');
        term_invert_cell(PROG_UI_START_COL, PROG_TOP_UI_BAR_ROW);
        mod_star = false;
    }
    
}

static void _uedit_draw_save_prompt(void) {
    term_clear_line(TERM_NUM_ROWS-1);
    term_draw_string(1, TERM_NUM_ROWS-1, "File modified! Save before exiting? y/n:");
    term_invert_line(TERM_NUM_ROWS-1);
}

