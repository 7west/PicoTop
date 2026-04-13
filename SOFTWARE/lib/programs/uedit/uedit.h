#pragma once

#include "prog.h"
#include "shell.h"
#include "vfs.h"


#define UEDIT_NUM_TEXT_ROWS 28


// shared variables between uedit code files
typedef struct {
    file_desc_t fd;
    bool read_only;
    uint32_t file_size;
    uint32_t cursor;
    uint32_t vp_top_doc;
} uedit_core_t;

extern const prog_vtable_t uedit_prog;



void uedit_enter(void);

void uedit_exit(void);

void uedit_on_key(key_event_t key);

void uedit_tick(void);

/**
 * @brief Called by shell cmd. Saves off file path of file. Runs vfs_open()
 * 
 * @param file - file path (MAX 256 CHAR)
 * @param ro - open in read only
 * @param force - force open file (for System files)
 * @param out - for printing errors
 * @return false - failed to open the file
 */
bool uedit_pass_file(const char *file, bool ro, bool force, const out_t *out);

/**
 * @brief Saves current file by creating FILENAME.TMP, writing all bytes from document
 * model to it, then overwriting original file.
 */
static bool _uedit_save_file(void);

// handles all keyboard input
static void _uedit_input_layer(key_event_t key);

static void _uedit_help_input(key_event_t key);

static void _uedit_stats_input(key_event_t key);

static void _uedit_save_input(key_event_t key);


// UI STUFF

// draws file name in top UI bar and shortcuts in lower UI bar
static void _uedit_draw_init_ui(void);

static void _uedit_draw_bottom_ui(void);

// adds "modification star" to file name. Sets mod_start var
static void _uedit_draw_mod_star(void);

// removes "modification star" to file name. Clears mod_start var
static void _uedit_clear_mod_star(void);

static void _uedit_draw_save_prompt(void);