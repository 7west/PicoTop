// uedit_view handles the Viewport domain for uEdit:
//      28 rows of text
//      gray cursor indicator

#include "uedit_view.h"

#include "os_debug.h"
#include "shell.h"
#include "uedit_doc.h"

// buffer of max number of chars that can be displayed on screen
#define VP_SCROLL_LINES 1
#define VP_PAGE_LINES 26 // lines scrolled in a PgUp/PgDn
#define VP_TAB_SPACES 4
#define VP_COPY1_COL 45
#define VP_CURSOR_CORNER_COL (TERM_NUM_COLS - 11)


static uedit_core_t *core;
static uedit_view_t *view;

static vp_row_t *rows;
static uint8_t *vp_cache;
// a literal because we want this in flash
static const char space_row[] = "                                                                                ";

// The cursor exists INSIDE the viewport. 
//      So vcursory == term_get_ycursor() + 1
static uint8_t vcursorx = 0;
static uint8_t vcursory = 0;

/**
 * @brief fills in one row with all necessary parameters. REQUIRES vp_cache be filled in
 * before calling. Be sure to pad last byte in vp_cache
 * 
 * @param r row struct to be filled
 * @param cache_offset where to read row start INSDIE vp_cache
 * @param cache_len maximum vp_cache length (so we don't read past it or stop at EOF)
 * @return uint32_t file bytes 'consumed' in this row
 */
static uint32_t _vp_calc_row(vp_row_t *r, uint32_t cache_offset, uint32_t cache_len);

/**
 * @brief Fills in one row, but will read directly from document model
 * 
 * @param r row struct to be filled
 * @param start_doc_pos position in document for start of this row
 * @return uint32_t uint32_t file bytes 'consumed' in this row
 */
static uint32_t _vp_calc_row_from_doc(vp_row_t *r, uint32_t start_doc_pos);




void vp_init(uedit_core_t *c, uedit_view_t *v) {
    core = c;
    view = v;

    vcursorx = 0;
    vcursory = 0;

    rows = view->rows_mem;
    vp_cache = view->vp_cache_mem;
}

/******************************************
 * vp_calc_all_rows() and helpers
*******************************************/

void vp_calc_all_rows(void) {

    // if viewport outside of file, mark all rows invalid
    if (core->vp_top_doc >= core->file_size) {
        for (uint8_t r = 0; r < UEDIT_NUM_TEXT_ROWS; r++) {
            rows[r].valid = false;
        }
        return;
    }

    // mark all rows initially as invalid
    for (uint8_t r = 0; r < UEDIT_NUM_TEXT_ROWS; r++) {
        rows[r].valid = false;
    }

    // calculate length of bytes to cache (bounded by file size)
    uint32_t remaining = core->file_size - core->vp_top_doc;
    uint32_t cache_len = remaining < VP_BUF_SIZE ? remaining : VP_BUF_SIZE;

    // fill the cache from document model
    if (!doc_read_file(core->vp_top_doc, vp_cache, cache_len)) {
        debug_deposit("UEDT-CRIT-Failed to calculate rows at doc_read_file()", 0, DBG_NULL_VAR);
        return;
    }
    vp_cache[cache_len] = 0x20;
    
    uint8_t row_idx = 0;
    rows[row_idx].doc_pos_start = core->vp_top_doc;

    uint32_t cache_offset = 0;

    while (row_idx < UEDIT_NUM_TEXT_ROWS) {

        // fill rows.cells[] with characters
        uint32_t bytes_read = _vp_calc_row(&rows[row_idx], cache_offset, cache_len);

        if (bytes_read) {
            cache_offset += bytes_read;

            // -1 because pos_end is inclusive
            rows[row_idx].doc_pos_end = rows[row_idx].doc_pos_start + bytes_read - 1;

            if (row_idx < UEDIT_NUM_TEXT_ROWS-1) {
                rows[row_idx + 1].doc_pos_start = rows[row_idx].doc_pos_end + 1;
            }

        } else {
            break;
        }
        row_idx += 1;
    }
}

static uint32_t _vp_calc_row(vp_row_t *r, uint32_t cache_offset, uint32_t cache_len) {

    uint8_t col = 0;
    uint8_t last_space = 0;
    uint32_t last_space_byte = 0;
    uint32_t bytes_read = 0;

    r->only_nl = false;

    uint8_t symbol;
    // <= means it will over fill row, but this is needed to capture last \n if necessary
    //      vp_cache[cache_len] should be set to 0x20, to help this!!!
    while (cache_offset <= cache_len && col < TERM_NUM_COLS) {

        symbol = vp_cache[cache_offset];
        // printf("row start: %u\n", r->doc_pos_start);
        // printf("symbol = %02X\n", symbol);
        bytes_read += 1;
        cache_offset += 1;

        if (symbol == '\n') {
            last_space = 0; // forces word wrap check to not change anything
            break; // new line, so end line printing

        } else if (symbol == '\t') { // render tab
            
            r->cells[col] = 0x09; // == \t in hex
            col += 1;

            while(((col % VP_TAB_SPACES) != 0) && col < TERM_NUM_COLS) {
                r->cells[col] = 0x0B; // == \v in hex
                    // this is the continuation byte for tabs, aids in parsing cells[] later
                col += 1;
            }

        } else if (symbol == '\r') { // we might do something special with \r later
            r->cells[col] = '\r';
            col += 1;
        
        } else if (symbol & 0x80) { // not an ASCII character
            r->cells[col] = 0x04; // diamond symbol in lieu of: �
            col += 1;

        } else { // any other character in a text file
            if (symbol == ' ') { // store last space for cleaner wrapping
                last_space = col;
                last_space_byte = bytes_read;
            }
            r->cells[col] = symbol;
            col += 1;
        }
    }

    if (symbol == ' ' && col == 1) r->only_nl = true;

    //  lines can start with spaces, last col sometimes has character:
    // if ((last_space != 0) && (vp_cache[cache_offset] != ' ')) { 
    //  lines never start with spaces, last col always blank
    if ((last_space != 0)) { 
        bytes_read = last_space_byte;
        col = last_space;
    }
    
    r->valid = (bytes_read > 0);

    memset(r->cells + col, 0x20, TERM_NUM_COLS - col);
    r->cells[TERM_NUM_COLS] = 0x00;
    r->cell_len = col;

    return bytes_read;
}


/******************************************
 * vp_render_rows() and helpers
*******************************************/

void vp_render_rows(void) {

    for (uint8_t r = 0; r < UEDIT_NUM_TEXT_ROWS; r++) {
        if (rows[r].valid) {
            term_draw_string(0, r + 1, rows[r].cells);
        } else {
            term_draw_string(0, r + 1, space_row);
        }
        
    }
}


/******************************************
 * vp_scroll_check() and helpers
*******************************************/
void vp_scroll_check(uint8_t symbol) {
    if (vcursory != UEDIT_NUM_TEXT_ROWS-1) {
        return;
    }

    if (symbol == '\n' || vcursorx == TERM_NUM_COLS-1 
        || (symbol == '\t' && vcursorx > TERM_NUM_COLS-1 - VP_TAB_SPACES))  {
            
        _vp_scroll_down();
    }
}



/******************************************
 * vp_render_vcursor() and helpers
*******************************************/

void vp_render_vcursor(void) {

    // find the last valid row
    uint8_t last_valid_row = _vp_get_last_row();

    // === Check if we need to scroll to find core->cursor
    
    // cursor at file_size is a valid position and should NOT prompt a scroll,
    //      but rows.doc_pos_end are inclusive, so file_size is not on a row

    // while (core->cursor > rows[last_valid_row].doc_pos_end && core->cursor != core->file_size) {
    //  ^^ commented out this line for vp_render_vcursor() for copy-paste. Be careful.
    while (core->cursor > rows[last_valid_row].doc_pos_end) {

        _vp_scroll_down();
        last_valid_row = _vp_get_last_row();

    }

    while (core->cursor < rows[0].doc_pos_start) {
        _vp_scroll_up();
    }
    last_valid_row = _vp_get_last_row();
    
    if (core->cursor < rows[0].doc_pos_start || (core->cursor > rows[last_valid_row].doc_pos_end + 1)) {
        // I have no idea how you would get here but it would be pretty bad
        //      The only sure solution is to close the text file and reopen it
        debug_deposit("UEDT-WARN-Failed to render cursor, viewport not encompass cursor val:", core->cursor, DBG_U32_DEC);
        return;
    }

    // Locate Cursor row on screen
    vcursory = 0;
    while (vcursory < last_valid_row && core->cursor >= rows[vcursory].doc_pos_end+1) { 
        vcursory += 1;
    }

    // === Locate cursor col on screen
    vcursorx = 0;
    uint32_t pos = 0;
    
    while (rows[vcursory].doc_pos_start + pos < core->cursor && rows[vcursory].cells[vcursorx] != 0x00) {

        if (vcursorx < TERM_NUM_COLS) {

            vcursorx += _vp_inc_row_cell(vcursory, vcursorx);
            pos += 1;

        } else {
            break; // something went very wrong
                // because cells[TERM_NUM_COLS (80)] should ALWAYS be 0x00.
        }
        
    }

    _vp_set_vcursor(vcursorx, vcursory);

}

static uint8_t _vp_get_last_row(void) {
    uint8_t last_valid_row = UEDIT_NUM_TEXT_ROWS-1;

    while(rows[last_valid_row].valid == false) {

        if (last_valid_row == 0) return 0;
        last_valid_row -= 1;
    }
    return last_valid_row;
}

static void _vp_set_vcursor(uint8_t x, uint8_t y) {
    // to account for top UI bar
    term_set_cursor(x, y + 1);
}

static uint8_t _vp_inc_row_cell(uint8_t r, uint8_t i) {
    
    uint8_t symbol = rows[r].cells[i];

    if (symbol == '\t') {
        return VP_TAB_SPACES - (i % VP_TAB_SPACES);
        
    } else if (symbol == '\v') {
        // something went very wrong. We crawl from the left so this should be impossible
        debug_deposit("UEDT-CRIT-_vp_inc_row_cell() detected \\v. cells[i], i=", i, DBG_U32_DEC);
        return 1; // fail safe 

    } else {
        return 1;
    }
}

/******************************************
 * vp_get_vcursory_cell() and helpers
*******************************************/

uint8_t vp_get_vcursor_cell(void) {
    return rows[vcursory].cells[vcursorx];
}


/******************************************
 * vp_cursor_down() and vp_scroll_down()
*******************************************/

void vp_cursor_down(void) {
    
    // printf("file size: %u\n", core->file_size);
    // printf("doc_pos_end: %u\n", rows[vcursory].doc_pos_end);
    // printf("cursor: %u\n", core->cursor);

    if (core->cursor == core->file_size) {
        // this is the last row of the file
        return;
    }

    // printf("vcursory = %u\n", vcursory);
    if (vcursory == UEDIT_NUM_TEXT_ROWS-1) { // We are at last row, we need to scroll down
        // printf("scrolling\n");
        // updates viewport and vcursor to reflect cursor has not changed
        _vp_scroll_down(); 
    } 
    
    
    //safety checks
    uint8_t last_valid_row = _vp_get_last_row();
    // printf("last valid row: %u\n", last_valid_row);
    if (vcursory >= last_valid_row) return;
    if (vcursory == UEDIT_NUM_TEXT_ROWS-1) return;
    if (!rows[vcursory + 1].valid) return;

    // === Moving Visual Cursor (vcursorx/y)
    if (vcursorx > rows[vcursory + 1].cell_len) { // next line is shorter
        vcursorx = rows[vcursory + 1].cell_len;

    } else if (rows[vcursory + 1].cells[vcursorx] == '\v') {
        vcursorx = vcursorx - (vcursorx % VP_TAB_SPACES);

    } else {
        // vcursorx does not change
    }

    // HERE!!!:
    // if (vcursorx == rows[vcursory + 1].cell_len && rows[vcursory + 1].doc_pos_end == core->file_size) {
    if (rows[vcursory + 1].only_nl && rows[vcursory + 1].doc_pos_end == core->file_size && vcursorx == rows[vcursory + 1].cell_len) {
        vcursorx -= 1;
    } 

    vcursory += 1;
    // printf("new vcy: %u\n", vcursory);

    // === Moving Cursor (core->cursor)

    uint8_t vcx_counter = 0;
    uint32_t pos_counter = 0;

    while (vcx_counter < vcursorx && vcx_counter < rows[vcursory].cell_len) {
        vcx_counter += _vp_inc_row_cell(vcursory, vcx_counter);
        pos_counter += 1;
    }
    
    core->cursor = pos_counter + rows[vcursory].doc_pos_start;
    _vp_set_vcursor(vcursorx, vcursory);

    // printf("Down done:\n");
    // printf("vcursor x: %u y: %u\n", vcursorx, vcursory);
    // printf("vcy row doc_pos_start: %u, doc_pos_end: %u\n", rows[vcursory].doc_pos_start, rows[vcursory].doc_pos_end);
    // printf("cursor: %u\n", core->cursor);
}

static void _vp_scroll_down(void) {

    uint8_t last_valid_row = _vp_get_last_row();
    if (!rows[last_valid_row].valid) return;
    if (1 > last_valid_row) return;
    if (!rows[1].valid) return;

    core->vp_top_doc = rows[1].doc_pos_start;

    uint32_t new_row_start = rows[UEDIT_NUM_TEXT_ROWS-1].doc_pos_end + 1;

    for (uint8_t i = 1; i < UEDIT_NUM_TEXT_ROWS; i++) {
        rows[i - 1] = rows[i];
    }

    uint32_t bytes = _vp_calc_row_from_doc(&rows[UEDIT_NUM_TEXT_ROWS-1], new_row_start);
    if (bytes == 0) rows[UEDIT_NUM_TEXT_ROWS-1].valid = false;

    vp_render_rows();

    // I'm not sure why this is here. It looks like it just -1 the vcursory
    vcursory = (vcursory > 1) ? (vcursory - 1) : 0;    
    _vp_set_vcursor(vcursorx, vcursory);
}


/******************************************
 * vp_cursor_up() and vp_scroll_up()
*******************************************/

void vp_cursor_up(void) {
    if ((rows[vcursory].doc_pos_start == 0) || core->cursor == 0) {
        // this is the first row of the file
        return;
    }

    if (vcursory == 0) { //we are at the first line, we need to scroll up
        _vp_scroll_up();
    }

    //safety checks
    if (vcursory <= 0) return;
    if (!rows[vcursory - 1].valid) return;

    // === Moving Visual Cursor
    if (vcursorx > rows[vcursory - 1].cell_len) {
        vcursorx = rows[vcursory - 1].cell_len;
    
    } else if (rows[vcursory - 1].cells[vcursorx] == '\v') {
        vcursorx = vcursorx - (vcursorx % VP_TAB_SPACES);
    } else {
        // vcursorx does not change
    }

    vcursory -= 1;

    // === Moving core Cursor

    uint8_t vcx_counter = 0;
    uint32_t pos_counter = 0;

    while (vcx_counter < vcursorx && vcx_counter < rows[vcursory].cell_len) {
        vcx_counter += _vp_inc_row_cell(vcursory, vcx_counter);
        pos_counter += 1;
    }

    core->cursor = pos_counter + rows[vcursory].doc_pos_start;
    _vp_set_vcursor(vcursorx, vcursory);

}

static void _vp_scroll_up(void) {

    if (core->vp_top_doc == 0) return;

    // we need to find doc_pos_start of rows[-1]. oof.
    uint32_t top_doc = core->vp_top_doc - 1;

    // == go backwards until we find \n or BOF
    uint32_t cache_len = 0;
    uint32_t scan_idx;
    uint32_t line_start_doc;
    uint32_t parse_idx = 0;
    bool break_found = false;

    // ensure byte at vp_top_doc-1 is not \n
    uint32_t search_end = core->vp_top_doc;
    if (core->vp_top_doc > 0) {
        uint8_t last;
        if (!doc_read_file(search_end - 1, &last, 1)) {
            debug_deposit("UEDT-CRIT-scroll up failed to read byte before vp_top_doc", 0, DBG_NULL_VAR);
            return;
        }
        if (last == '\n') search_end -= 1;
    }

    while(!break_found) {

        // we need to grab all the chars before vp_top_doc
        cache_len = search_end < VP_BUF_SIZE ? search_end : VP_BUF_SIZE;
        uint32_t window_start = search_end - cache_len;

        if (!doc_read_file(window_start, vp_cache, cache_len)) {
            debug_deposit("UEDT-CRIT-scroll up failed at large doc_read_file()", 0, DBG_NULL_VAR);
            return;
        }
        vp_cache[cache_len] = 0x20;

        // we hit Beginning of File
        if (window_start == 0) {
            line_start_doc = 0;
            break_found = true;
            break;
        }

        scan_idx = 1;
        while (cache_len >= scan_idx) {

            // LLM says "scan_idx != 1" is fragile, but I don't see how. Potential issue noted.
            if (vp_cache[cache_len - scan_idx] == '\n') {

                break_found = true;
                uint32_t new_line_idx = cache_len - scan_idx;
                line_start_doc = window_start + new_line_idx + 1;
                break;
            }

            scan_idx += 1;            
        }

        search_end = window_start;
    }

    // === now find doc_pos_start & _end until we match top_doc
    vp_row_t temp_row = {0};
    uint32_t pos = line_start_doc;

    while(1) {
        uint32_t consumed = _vp_calc_row_from_doc(&temp_row, pos);
        if (consumed == 0) {
            // should NOT happen. EOF/bug. fail safe:
            core->vp_top_doc = line_start_doc;
            break;
        }

        if (temp_row.doc_pos_end == top_doc) {
            // found rows[-1]
            core->vp_top_doc = temp_row.doc_pos_start;
            break;
        }

        if (temp_row.doc_pos_end > top_doc) {
            // should NOT happen. vp_dop_doc was not aligned. Fail safe:
            core->vp_top_doc = temp_row.doc_pos_start;
            break;
        }

        pos = temp_row.doc_pos_end + 1;
    }

    // === scroll up by shifting rows down and entering new one (temp_row) at top
    memmove(&rows[1], &rows[0], (UEDIT_NUM_TEXT_ROWS - 1) * sizeof(vp_row_t));
    rows[0] = temp_row;

    vp_render_rows();

    vcursory += 1;
    _vp_set_vcursor(vcursorx, vcursory);
}

static uint32_t _vp_calc_row_from_doc(vp_row_t *r, uint32_t start_doc_pos) {

    if (start_doc_pos > core->file_size) return 0;

    uint32_t remaining = core->file_size - start_doc_pos;
    // This +1 is CRUCIAL: it allows a certain check for wrapping in _vp_calc_row()
    uint32_t to_read = remaining < TERM_NUM_COLS+1 ? remaining : TERM_NUM_COLS+1;

    r->doc_pos_start = start_doc_pos;

    // if (to_read == 0) {
    //     r->valid = false;
    //     r->cell_len = 0;
    //     memset(r->cells, 0x20, TERM_NUM_COLS);
    //     r->cells[TERM_NUM_COLS] = 0x00;
    //     return 0;
    // }
    
    if (to_read > 0) {
        if (!doc_read_file(start_doc_pos, vp_cache, to_read)) {
            return 0;
        }
    }
    vp_cache[to_read] = 0x20;

    uint32_t consumed = _vp_calc_row(r, 0, to_read);
    if (consumed == 0) return 0;

    r->doc_pos_end = r->doc_pos_start + consumed - 1;

    return consumed;
}

/******************************************
 * vp_set_copy_start() & _end() & helpers
*******************************************/

void vp_set_copy_start(void) {
    term_draw_char_inv(VP_COPY1_COL, TERM_NUM_ROWS-1, '1');
    term_invert_cell(vcursorx, vcursory + 1);

    view->vp_copy.vcx_copy1 = vcursorx;
    view->vp_copy.vcy_copy1 = vcursory;
    view->vp_copy.top_doc1 = core->vp_top_doc;
}

void vp_set_copy_end(void) {
    term_draw_char_inv(VP_COPY1_COL + 1, TERM_NUM_ROWS-1, '2');
    term_invert_cell(vcursorx, vcursory + 1);

    view->vp_copy.vcx_copy2 = vcursorx;
    view->vp_copy.vcy_copy2 = vcursory;
    view->vp_copy.top_doc2 = core->vp_top_doc;
}

void vp_clear_copy_start(void) {
    term_draw_char_inv(VP_COPY1_COL, TERM_NUM_ROWS-1, ' ');
    if (core->vp_top_doc == view->vp_copy.top_doc1) {
        term_invert_cell(view->vp_copy.vcx_copy1, view->vp_copy.vcy_copy1 + 1);
    }
}

void vp_clear_copy_end(void) {
    term_draw_char_inv(VP_COPY1_COL + 1, TERM_NUM_ROWS-1, ' ');
    if (core->vp_top_doc == view->vp_copy.top_doc2) {
        term_invert_cell(view->vp_copy.vcx_copy2, view->vp_copy.vcy_copy2 + 1);
    }
}

/******************************************
 * vp_cursor_home() and vp_cursor_end()
*******************************************/

void vp_cursor_home(void) {
    vcursorx = 0;
    core->cursor = rows[vcursory].doc_pos_start;

    _vp_set_vcursor(vcursorx, vcursory);
}

// TODO: FIX!!! If last line is \n, vcursorx goes +1 too far
void vp_cursor_end(void) {
    vcursorx = rows[vcursory].cell_len;

    // wrapped lines will have cel_len == 80, clamp it to 79
    if (vcursorx == TERM_NUM_COLS) {
        vcursorx = TERM_NUM_COLS - 1; 
    }
    // vcursorx -= 1;
    core->cursor = rows[vcursory].doc_pos_end;
    // HERE!!!:
    // if (core->cursor == core->file_size && vcursorx == 0) vcursorx -= 1;
    if (rows[vcursory].only_nl && core->cursor == core->file_size) vcursorx -= 1;

    _vp_set_vcursor(vcursorx, vcursory);
}

/******************************************
 * vp_cursor_pgdn() and vp_cursor_pgup()
*******************************************/

void vp_cursor_pgdn(void) {

    for (uint8_t i = 0; i < VP_PAGE_LINES; i++) {

        if (vcursory == UEDIT_NUM_TEXT_ROWS-1) {
            _vp_scroll_down();
            vp_cursor_down();
        } else {
            vp_cursor_down();
            _vp_scroll_down();
        }
    } 
}

void vp_cursor_pgup(void) {
    
    for (uint8_t i = 0; i < VP_PAGE_LINES; i++) {

        if (vcursorx == UEDIT_NUM_TEXT_ROWS-1) {
            vp_cursor_up();
            _vp_scroll_up();
        } else {
            _vp_scroll_up();
            vp_cursor_up();
        }
    }
    // TODO: one day we will optimize this by incorporating the 
    //      _vp_scroll_up() logic and instead of doing one line
    //      at a time, we will store all the doc_pos_start values
    //      so we can save them off and not do the whole search
    //      once per line
    //      this would be pretty complex and hard, so it is a loooong term goal
}

/******************************************
 * vp_cursor_bottom() and helpers
*******************************************/

void vp_cursor_bottom(void) {

    if (core->file_size == 0) return;

    // === First check if EOF is already in the window
    uint8_t last_valid_row = _vp_get_last_row();
    if (rows[last_valid_row].doc_pos_end + 1 == core->file_size) {

        vcursory = last_valid_row;
        vp_cursor_end();
        core->cursor = core->file_size;
        return;
    }

    // == go backwards until we find \n or BOF
    uint32_t cache_len = 0;
    uint32_t scan_idx;
    uint32_t line_start_doc;
    uint32_t parse_idx = 0;
    bool break_found = false;

    bool last_char_nl = false;

    // ensure byte at file_size-1 is not \n
    uint32_t search_end = core->file_size;
    if (core->file_size > 0) {
        uint8_t last;
        if (!doc_read_file(search_end - 1, &last, 1)) {
            debug_deposit("UEDT-CRIT-scroll up failed to read byte before vp_top_doc", 0, DBG_NULL_VAR);
            return;
        }
        if (last == '\n') {
            last_char_nl = true;
            search_end -= 1;
        }
    }

    while(!break_found) {

        // we need to grab all the chars before vp_top_doc
        cache_len = search_end < VP_BUF_SIZE ? search_end : VP_BUF_SIZE;
        uint32_t window_start = search_end - cache_len;

        if (!doc_read_file(window_start, vp_cache, cache_len)) {
            debug_deposit("UEDT-CRIT-scroll up failed at large doc_read_file()", 0, DBG_NULL_VAR);
            return;
        }

        vp_cache[cache_len] = ' ';

        // we hit Beginning of File
        if (window_start == 0) {
            line_start_doc = 0;
            break_found = true;
            break;
        }

        scan_idx = 1;
        while (cache_len >= scan_idx) {

            if (vp_cache[cache_len - scan_idx] == '\n') {

                break_found = true;
                uint32_t new_line_idx = cache_len - scan_idx;
                line_start_doc = window_start + new_line_idx + 1;
                break;
            }

            scan_idx += 1;            
        }

        search_end = window_start;
    }

    // === now find doc_pos_start & _end until we match top_doc
    vp_row_t temp_row = {0};
    uint32_t pos = line_start_doc;

    while(1) {
        uint32_t consumed = _vp_calc_row_from_doc(&temp_row, pos);
        if (consumed == 0) {
            // should NOT happen. EOF/bug. fail safe:
            core->vp_top_doc = line_start_doc;
            break;
        }

        if (temp_row.doc_pos_end == (core->file_size - 1)) {
            // found rows[-1]
            core->vp_top_doc = temp_row.doc_pos_start;
            break;
        }

        if (temp_row.doc_pos_end > (core->file_size - 1)) {
            // should NOT happen. vp_dop_doc was not aligned. Fail safe:
            core->vp_top_doc = temp_row.doc_pos_start;
            break;
        }

        pos = temp_row.doc_pos_end + 1;
    }

    core->vp_top_doc = temp_row.doc_pos_start;
    
    // // === wipe rows and put (temp_row) at top
    // for (uint8_t i = 0; i < UEDIT_NUM_TEXT_ROWS; i++) {
    //     rows[i].valid = false;
    // }
    // rows[0] = temp_row;

    vp_calc_all_rows();

    vp_render_rows();
    
    core->cursor = core->file_size;
    vp_render_vcursor();

}


/******************************************
 * vp_draw_help() and vp_draw_stats() and vp_update_cursor_corner()
 * and helpers
*******************************************/

void vp_draw_help(void) {
    term_clear_prog_screen();

    term_draw_string(3, 2, "Shortcuts");
    term_draw_string(3, 3, "=========");
    term_draw_string(3, 4, "Ctrl+Home - start of file");
    term_draw_string(3, 5, "Ctrl+End - End of file");
    term_draw_string(3, 6, "Ctrl+ /  - jump words");
    term_draw_char(8, 6, 0x1B);
    term_draw_char(10, 6, 0x1A);
    term_draw_string(3, 7, "");
    term_draw_string(3, 8, "Copy Paste Guidance");
    term_draw_string(3, 9, "===================");
    term_draw_string(3, 10, "1. Move cursor to start of text to copy");
    term_draw_string(3, 11, "2. Press F3");
    term_draw_string(3, 12, "3. Move cursor to end of text to copy");
    term_draw_string(3, 13, "3. Press F4");
    term_draw_string(3, 14, "4. Move cursor to where you want to paste");
    term_draw_string(3, 15, "5. Press F5");
    term_draw_string(3, 16, "You cannot paste inside the copy selection. The 'C12' in the bottom UI bar");
    term_draw_string(3, 17, "indicates whether the start and end copy positions have been set.");

    term_draw_string(1, TERM_NUM_ROWS-1, "F1 to exit help");
    term_invert_line(TERM_NUM_ROWS-1);
}

void vp_draw_stats(void) {
    char num_string[17] = {0};

    term_clear_prog_screen();

    term_draw_string(3, 2, "Calculating Stats...");

    uint32_t word_count = doc_calc_word_count();
    uint8_t mem_usage = doc_calc_mem_usage();
    uint8_t ptable_usage = doc_calc_ptable_usage();
    uint32_t para_count = doc_calc_para_count();

    term_draw_string(24, 2, "Done");

    term_draw_string(3, 4, "File Size:");
    snprintf(num_string, 17, "%u bytes", core->file_size);
    term_draw_string(14, 4, num_string);

    term_draw_string(3, 5, "Word Count:");
    snprintf(num_string, 17, "%u", word_count);
    term_draw_string(15, 5, num_string);

    term_draw_string(3, 6, "Paragraph Count:");
    snprintf(num_string, 17, "%u", para_count);
    term_draw_string(20, 6, num_string);

    term_draw_string(2, 8, "== Advanced Stats ==");

    term_draw_string(3, 9, "Mem Usage:");
    snprintf(num_string, 17, "%u%%", mem_usage);
    term_draw_string(14, 9, num_string);

    term_draw_string(3, 10, "Piece Table Usage:");
    snprintf(num_string, 17, "%u%%", ptable_usage);
    term_draw_string(22, 10, num_string);

    term_draw_string(1, TERM_NUM_ROWS-1, "F2 to exit stats");
    term_invert_line(TERM_NUM_ROWS-1);
}

void u32toa_right_align(uint32_t value, char* buffer) {
    // Null terminate the very end of the 10-character field
    buffer[10] = '\0';

    // Fill from right to left (indices 9 down to 0)
    for (int i = 9; i >= 0; i--) {
        if (value > 0 || i == 9) { // Ensure at least one '0' if value is 0
            buffer[i] = (value % 10) + '0';
            value /= 10;
        } else {
            buffer[i] = ' '; // Fill remaining leading space with ' '
        }
    }
}

void vp_update_cursor_corner(void) {
    uint32_t cursor = core->cursor;
    uint8_t x = TERM_NUM_COLS - 1;
    uint8_t y = TERM_NUM_ROWS - 1;

    for (uint8_t i = 0; i < 10; i++) {

        if (cursor > 0 || i == 0) {
            term_draw_char_inv(x, y, (cursor % 10) + '0');
            cursor /= 10;

        } else {
            term_draw_char_inv(x, y, ' ');
        }

        x -= 1;
    }

}