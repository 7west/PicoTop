// uedit_view handles the Viewport domain for uEdit:
//      28 rows of text
//      gray cursor indicator

#pragma once

#include "uedit.h"
#include "terminal.h"

#define VP_BUF_SIZE (UEDIT_NUM_TEXT_ROWS * TERM_NUM_COLS)

// Row struct with all the properties a row could need
typedef struct {

    uint32_t doc_pos_start;
    uint32_t doc_pos_end; // INCLUSIVE!!!
    char cells[TERM_NUM_COLS + 1];
    uint8_t cell_len;
    bool valid;
    bool dirty;
    bool only_nl;
    
} vp_row_t;

typedef struct {

    uint8_t vcx_copy1;
    uint8_t vcy_copy1;
    uint8_t vcx_copy2;
    uint8_t vcy_copy2;
    uint32_t top_doc1;
    uint32_t top_doc2;

} vp_copy_store_t;


typedef struct {

    vp_row_t rows_mem[UEDIT_NUM_TEXT_ROWS];
    uint8_t vp_cache_mem[VP_BUF_SIZE+1];
    vp_copy_store_t vp_copy;

} uedit_view_t;


// declared in uedit_view.c
// uint32_t _vp_calc_row(vp_row_t *r, uint32_t cache_offset, uint32_t cache_len);
// uint32_t _vp_calc_row_from_doc(vp_row_t *r, uint32_t start_doc_pos);


// initializes statics for Viewport
void vp_init(uedit_core_t *c, uedit_view_t *v);

// calculates all rows. MUST set core->vp_top_doc before calling
void vp_calc_all_rows(void);

// prints all rows to the terminal. Only prints "valid" rows
void vp_render_rows(void);


////////////////////////////

/**
 * @brief checks if character you are about to enter will trigger a scroll down
 * If yes, it scrolls the viewport down one line
 * 
 * @param symbol checked for chars that would cause wrap/new line
 */
void vp_scroll_check(uint8_t symbol);


////////////////////////////

/**
 * @brief Moves viewport until it contains core->cursor then it draws the visual cursor
 * at that position, changing vcursorx & vcursory to reflect the new position
 * 
 * This function is a real pain in the ass. I made a lot of patches and catches to make it work.
 * It is not pretty, but it works... for now
 * 
 */
void vp_render_vcursor(void);

/**
 * @brief Returns the number of visual cells the occupied by character at rows[r].cells[i]
 * 
 * Mainly handles converting moving one document byte to how many cells across the Viewport.
 * Pretty much for tabs
 * 
 * @param r row in Viewport
 * @param i index in cells[]
 * @return uint8_t number of cells occupied by character
 */
static uint8_t _vp_inc_row_cell(uint8_t r, uint8_t i) ;

// draws visual cursor. Handles Viewport => Terminal row math
static void _vp_set_vcursor(uint8_t x, uint8_t y);

// returns the last valid row in the view port by checking "valid" flag of rows
static uint8_t _vp_get_last_row(void);


//////////////////////////

// returns the on screen character in cells[] at the current visual cursor position
uint8_t vp_get_vcursor_cell(void);


////////////////////////////

/**
 * @brief Handles arrow key down from user. Moves core->cursor as well
 * 
 * Keeps vcursorx constant, so it has to calculate what that new vcursorx means for the 
 * core->cursor. Will _vp_scroll_down() if necessary
 * 
 */
void vp_cursor_down(void);

// fills in new bottom row and shifts all other rows up
//      keeps visual cursor on the same character in document
static void _vp_scroll_down(void);

///////////////////////////

/**
 * @brief Handles arrow key up from user. Moves core->cursor as well
 * 
 * Keeps vcursorx constant, so it has to calculate what that new vcursorx means for the 
 * core->cursor. Will _vp_scroll_up() if necessary
 * 
 */
void vp_cursor_up(void);

// due to how complex scrolling up is, if you want multiple scrolls, you need to loop this command.

/**
 * @brief Quite complicated function, necessarily so to keep row wrapping consistent when scrolling up
 * 
 * due to how complex scrolling up is, if you want multiple scrolls, you need to loop this command.
 * 
 * Scans up from core->vp_top_doc to find previous \n or BOF. Once it finds it, it works forward
 * filling temp_row until temp_row.doc_pos_end + 1 == core->vp_top_doc
 * 
 */
static void _vp_scroll_up(void);

///////////////////////////

void vp_set_copy_start(void);

void vp_set_copy_end(void);

void vp_clear_copy_start(void);

void vp_clear_copy_end(void);

///////////////////////////

// moves visual cursor and core->cursor to beginning of line
void vp_cursor_home(void);

// moves visual cursor and core->cursor to end of line. Small check for wrapped lines
void vp_cursor_end(void);

///////////////////////////

// series of _vp_scroll_down() & vp_cursor_down() calls
void vp_cursor_pgdn(void);

// series of _vp_scroll_up()) & vp_cursor_up() calls
void vp_cursor_pgup(void);

///////////////////////////

// moves cursor to last line and position in the file. Uses similar complex calculating as
// _vp_scroll_up()
void vp_cursor_bottom(void);

///////////////////////////

void vp_draw_help(void);

void vp_draw_stats(void);

void vp_update_cursor_corner(void);