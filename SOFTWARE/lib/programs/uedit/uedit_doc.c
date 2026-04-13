// uedit_doc handles the Document Model for uEdit:
//      BASE and ADD records in Piece Table (ptable)
//      holding current copy of file's byte stream

#include "uedit_doc.h"

#include "vfs.h"
#include "os_debug.h"
#include "terminal.h"
#include "shell.h"
#include "uedit_view.h"
#include "buzz.h"

// #define printf(...) ((void)0)

// Piece Table Entry type
typedef enum {
    PTABLE_SRC_INVALID,
    PTABLE_SRC_BASE,
    PTABLE_SRC_ADD,
} ptable_src_t;


static uedit_core_t *core;
static uedit_doc_t *doc;

static ptable_record_t *ptable;
static ptable_record_t *copy_ptable;
static uint8_t *ADD_buffer;

static uint16_t last_record; // +1 and you get number of records
static uint16_t last_in_ADD; // index of last record in the ADD buffer
static uint32_t ADD_tail;

static uint32_t doc_copy1;
static uint32_t doc_copy2;



// takes current core->cursor position
//      if at EOF, returns record index of last record with offset == record.len
//      if bad error, returns record index of PTABLE_NUM_RECORDS
static record_offset_t _doc_get_rec_offset(uint32_t pos);


void doc_init(uedit_core_t *c, uedit_doc_t *d) {
    core = c;
    doc = d;

    ptable = doc->ptable_mem;
    copy_ptable = doc->copy_ptable_mem;
    ADD_buffer = doc->ADD_buffer_mem;

    last_record = 0;
    ADD_tail = 0;
    last_in_ADD = PTABLE_NUM_RECORDS + 1;

    doc_copy1 = 0xFFFFFFFF;
    doc_copy2 = 0xFFFFFFFF;

    memset(ptable, 0x00, sizeof(ptable_record_t) * PTABLE_NUM_RECORDS);

    ptable[0] = (ptable_record_t) {
        .src = PTABLE_SRC_BASE,
        .offset = 0,
        .len = core->file_size,
    };
}

void doc_reset_ptable(void) {
    last_record = 0;
    ADD_tail = 0;
    last_in_ADD = PTABLE_NUM_RECORDS + 1;

    // clear ADD buffer?

    memset(ptable, 0x00, sizeof(ptable_record_t) * PTABLE_NUM_RECORDS);

    ptable[0] = (ptable_record_t) {
        .src = PTABLE_SRC_BASE,
        .offset = 0,
        .len = core->file_size,
    };
}

/////////
// Reading Piece Table

bool doc_read_file(uint32_t offset, uint8_t *buf, uint32_t len) {

    // === Safety checks
    if (len == 0) return true;
    if (offset > core->file_size - len) return false;

    // === walk through ptable records until at record with desired offset
    uint16_t record = 0;
    uint32_t current_pos = 0;

    while (record < PTABLE_NUM_RECORDS) {
        // this is new code to catch the "new file hang" bug. Placed 2026-03-05
        if (current_pos + ptable[record].len < current_pos) return false;

        if (current_pos + ptable[record].len > offset) break;

        current_pos += ptable[record].len;
        record += 1;
    }
    if (record == PTABLE_NUM_RECORDS) {
        debug_deposit("UEDT-CRIT-doc_file_read() failed to find cursor in Piece Table:", core->cursor, DBG_U32_DEC);
        return false;
    }

    // === Write ptable records to buf
    uint32_t record_offset = offset - current_pos;
    uint8_t *write_ptr = buf;

    while (len > 0 && record < PTABLE_NUM_RECORDS) {

        uint32_t bytes_avail = ptable[record].len - record_offset;
        uint32_t bytes_to_write = (len < bytes_avail) ? len : bytes_avail;

        if (!_doc_transfer_bytes(record, record_offset, write_ptr, bytes_to_write)) {
            return false;
        }

        record_offset = 0;
        len -= bytes_to_write;
        write_ptr += bytes_to_write;
        record += 1;
    }

    return (len == 0);
}


static bool _doc_transfer_bytes(uint16_t record, uint32_t internal_offset, uint8_t *buf, uint32_t len) {

    // safety checks
    // if (len == 0) return false;
    // if (internal_offset > ptable[record].len - len) return false;

    uint32_t abs_source_addr = ptable[record].offset + internal_offset;

    // transfer the bytes depending on source
    if (ptable[record].src == PTABLE_SRC_BASE) {
        if (!vfs_seek(core->fd, abs_source_addr, FS_SEEK_START)) {
            debug_deposit("UEDT-CRIT-Attempted invalid vfs_seek() value in _doc_transfer_bytes:", abs_source_addr, DBG_U32_DEC);
            return false;
        }

        int32_t bytes_read = vfs_read(core->fd, buf, len);
        if (bytes_read < 0) {
            debug_deposit("UEDT-CRIT-vfs_read() failed to read from disk", 0, DBG_NULL_VAR);
            return false;
        } 
        if ((uint32_t)bytes_read != len) {
            debug_deposit("UEDT-WARN-vfs_read() returned less bytes than expected. bytes_read:", bytes_read, DBG_U32_DEC);
            return false;
        }

    } else if (ptable[record].src == PTABLE_SRC_ADD) { // source is ADD buffer
        memcpy(buf, ADD_buffer + abs_source_addr, len);

    } else {
        debug_deposit("UEDT-CRIT-ptable record in _doc_transfer_bytes() is SRC_INVALID", 0, DBG_NULL_VAR);
        return false;
    }
    
    return true;
}

void doc_stream_file_UART(void) {

    char buf[17] = {0};
    uint32_t bytes_left = core->file_size;

    while (bytes_left > 16) {
        doc_read_file(core->file_size - bytes_left, buf, 16);

        printf("%s", buf);

        bytes_left -= 16;
    }

    memset(buf, 0x00, 16);
    doc_read_file(core->file_size - bytes_left, buf, bytes_left);
    printf("%s", buf);

}

/////////
// Record Manipulation - Adding

bool doc_add_char(uint8_t symbol) {

    if (core->cursor == 0) {
        // printf("char @ beginning\n");

        // modify records
        if (!_doc_shift_rec_down(0, 1)) return false;
        
        if (!_doc_fill_new_record(0, symbol)) return false;

        // UI updates
        vp_scroll_check(symbol);
        vp_calc_all_rows();
        vp_render_rows();
        vp_render_vcursor();

        return true;
    }

    record_offset_t ro = _doc_get_rec_offset(core->cursor);
    if (ro.record == PTABLE_NUM_RECORDS) return false;

    // printf("cursor = %u\n", core->cursor);
    // printf("ro = rec: %u, rec_offset: %u\n", ro.record, ro.offset);

    if (ro.offset < ptable[ro.record].len) { // we are INSDIE a record
        // printf("char @ Inside record\n");

        // modify records
        if (!_doc_split_record_add(ro.record, ro.offset)) return false;
        uint16_t new_rec = ro.record + 1;
        
        if (!_doc_fill_new_record(new_rec, symbol)) return false;

        // UI updates
        vp_scroll_check(symbol);
        vp_calc_all_rows();
        vp_render_rows();
        vp_render_vcursor();

        return true;

    } else if (ro.offset == ptable[ro.record].len) { // we are at the end of a record
        
        // special case where we can just append to ADD buffer without making a new record,
        // because last_in_ADD stores the record at the end of the ADD buffer
        //      technically it is a very common case
        if ((ptable[ro.record].src == PTABLE_SRC_ADD) && (ro.record == last_in_ADD)) {

            // printf("char @ optimal place\n");

            // add char to record
            // uint32_t ADD_addr = ptable[ro.record].offset + ptable[ro.record].len;
            ADD_buffer[ADD_tail] = symbol;
            ptable[ro.record].len += 1;

            // update statics & UI
            ADD_tail += 1;
            core->file_size += 1;
            core->cursor += 1;

            // UI updates
            vp_scroll_check(symbol);
            vp_calc_all_rows();
            vp_render_rows();
            vp_render_vcursor();

            return true;

        } else { // normal after ADD (when not at end of ADD buffer) or BASE record

            // printf("char @ After record\n");

            // modify records
            uint16_t new_rec = ro.record + 1;
            if (new_rec == PTABLE_NUM_RECORDS) {
                debug_deposit("UEDT-CRIT-adding record at end of Piece Table failed. Table Full!", 0, DBG_NULL_VAR);
                return false;
            }
            _doc_shift_rec_down(new_rec, 1);
            
            if (!_doc_fill_new_record(new_rec, symbol)) return false;

            // UI updates
            vp_scroll_check(symbol);
            vp_calc_all_rows();
            vp_render_rows();
            vp_render_vcursor();

            return true;
        }
    }
}

static bool _doc_fill_new_record(uint16_t new_rec, uint8_t symbol) {

    if (new_rec == PTABLE_NUM_RECORDS) {
        debug_deposit("UEDT-CRIT-adding & filling record at end of Piece Table failed. Table Full!", 0, DBG_NULL_VAR);
        return false;
    }
    
    ptable[new_rec] = (ptable_record_t) {
        .src = PTABLE_SRC_ADD,
        .offset = ADD_tail,
        .len = 1,
    };
    last_in_ADD = new_rec;

    ADD_buffer[ADD_tail] = symbol;

    ADD_tail += 1;
    core->file_size += 1;
    core->cursor += 1;

    return true;
}

static bool _doc_shift_rec_down(uint16_t rec, uint16_t shift_n) {

    // caller tried to shift down after last record, unecessary
    //      just start writing at ptable[rec]
    if (rec > last_record) { 
        last_record = last_record + shift_n;
        return true;
    }

    if (last_record + shift_n >= PTABLE_NUM_RECORDS) {
        debug_deposit("UEDT-CRIT-shifting records down failed. Piece Table Full! last_record:", last_record, DBG_U32_DEC);
        return false;
    }

    // (last_record - rec + 1) because this is inclusive
    //      e.g. (5 - 5) wouldn't work, we would need +1
    memmove(&ptable[rec + shift_n], &ptable[rec], (last_record - rec + 1) * sizeof(ptable_record_t));

    last_record += shift_n;
    return true;
}

static bool _doc_split_record_add(uint16_t rec, uint32_t rec_offset) {

    uint16_t og_rec = rec;

    if (!_doc_shift_rec_down(og_rec + 1, 2)) return false;

    // uint16_t new_rec = og_rec + 1;
    uint16_t og_rec2 = og_rec + 2;

    ptable[og_rec2].src = ptable[og_rec].src;
    ptable[og_rec2].offset = ptable[og_rec].offset + rec_offset;
    ptable[og_rec2].len = ptable[og_rec].len - rec_offset;

    ptable[og_rec].len = rec_offset;

    return true;    
}

static uint16_t _doc_get_record(uint32_t pos) {
    return _doc_get_rec_offset(pos).record;
}

static record_offset_t _doc_get_rec_offset(uint32_t pos) {

    uint32_t cursor_track = pos;
    record_offset_t ro = {0};
    // printf("last record = %u\n", last_record);

    while (ro.record <= last_record) {

        // printf("rec idx = %u\n", ro.record);
        // printf("cursor_track = %u, record_len = %u\n", cursor_track, ptable[ro.record].len);

        if (cursor_track == ptable[ro.record].len) {

            if (ro.record == last_record) { // invalid, probably means we're at EOF
                // TODO: maybe set this to actual last_record? and offset to len???
                // ro.record = PTABLE_NUM_RECORDS; 
                // ro.offset = 0xFFFFFFFF; // double invalid values
                ro.offset = ptable[ro.record].len;
                return ro;

            } else {
                // ro.record += 1; //commented because we need it for a check later
                ro.offset = cursor_track;
                return ro;
            }
            
        } else if (cursor_track < ptable[ro.record].len) {
            ro.offset = cursor_track;
            return ro;

        } else {
            cursor_track -= ptable[ro.record].len;
            ro.record += 1;
        }
    }

    debug_deposit("UEDT-CRIT-_doc_get_rec_offset(): core->cursor not found in cursor:", core->cursor, DBG_U32_DEC);
    // this is bad. normally this function should never return PTABLE_NUM_RECORDS, 
    //          so this value means failure
    ro.record = PTABLE_NUM_RECORDS;
    return ro;
}

/////////
// Record Manipulation - Removing

bool doc_bksp_char(void) {
    if (core->cursor == 0) return true; // do nothing, not an error

    _doc_rem_char(core->cursor - 1);

    core->cursor -= 1;

    vp_render_vcursor();
    vp_calc_all_rows();
    vp_render_rows();
    vp_render_vcursor();
    
}

bool doc_del_char(void) {
    if (core->cursor == core->file_size) return true; // do nothing, not an error

    _doc_rem_char(core->cursor);

    vp_calc_all_rows();
    vp_render_rows();
    vp_render_vcursor();
}

static bool _doc_rem_char(uint32_t pos) {

    record_offset_t ro = _doc_get_rec_offset(pos);

    // printf("rec offset: %u\n", ro.offset);
    // printf("rec len = %u\n", ptable[ro.record].len);

    // because len is exclusive, being at len means we are at Front of next record
    if (ro.offset == ptable[ro.record].len && ro.record != last_record) {
        ro.offset = 0;
        ro.record += 1;
    }

    if (ro.offset == 0 && ptable[ro.record].len != 1) { // Front of record

        // printf("rem char @ Front of record\n");

        ptable[ro.record].offset += 1;
        ptable[ro.record].len -= 1;
        core->file_size -= 1;

    } else if (ro.offset < ptable[ro.record].len - 1) { // Middle of record

        // printf("rem char @ Middle of record\n");

        _doc_split_record_rem(ro.record, ro.offset);
        core->file_size -= 1;
        
    } else if (ro.offset == ptable[ro.record].len - 1) { // End of record

        // printf("rem char @ End of record\n");

        // special case: char we are removing is at the end of the last record in ADD
        //      much more efficient, actually garbage collects in ADD
        if ((ptable[ro.record].src == PTABLE_SRC_ADD) && (ro.record == last_in_ADD)) {

            // printf("rem char @ optimal place\n");

            ptable[ro.record].len -= 1;
            ADD_tail -= 1;
            ADD_buffer[ADD_tail] = 0x00; //???? do we need this?
            core->file_size -= 1;

        } else { // more populous End of BASE or ADD record case

            ptable[ro.record].len -= 1;
            core->file_size -= 1;

        }

    } else {
        debug_deposit("UEDT-CRIT-Missed reasonable conditions in _doc_rem_char. wth? pos:", pos, DBG_U32_DEC);
        return false;
    }

    if (!_doc_trim_record(ro.record)) {
        debug_deposit("UEDT-WARN-record of len == 0 could not be removed. rec:", ro.record, DBG_U32_DEC);
        return false;
    }

    return true;
}

static bool _doc_shift_rec_up(uint16_t rec, uint16_t shift_n) {

    if (shift_n > rec) {
        debug_deposit("UEDT-CRIT-rec shift up failed. Can't shift rec up into negatives. rec:", rec, DBG_U32_DEC);
        return false;
    }

    memmove(&ptable[rec - shift_n], &ptable[rec], (last_record - rec + 1) * sizeof(ptable_record_t));

    last_record -= shift_n;
    return true;
}

static bool _doc_split_record_rem(uint16_t rec, uint32_t rec_offset) {

    uint16_t og_rec = rec;

    if (!_doc_shift_rec_down(og_rec + 1, 1)) return false;

    uint16_t og_rec2 = og_rec + 1;

    ptable[og_rec2].src = ptable[og_rec].src;
    ptable[og_rec2].offset = ptable[og_rec].offset + rec_offset + 1;
    ptable[og_rec2].len = ptable[og_rec].len - rec_offset - 1;

    ptable[og_rec].len = rec_offset;

    return true;
}

static bool _doc_trim_record(uint16_t rec) {

    if (ptable[rec].len == 0) {

        // remove the record
        if (rec != last_record) {
            if (!_doc_shift_rec_up(rec + 1, 1)) return false;

        } else { // rec with len=0 is last rec
            ptable[rec] = (ptable_record_t) {
                .src = PTABLE_SRC_INVALID,
                .offset = 0,
                .len = 0,
            };
            last_record -= 1;
        }
        
        // check if the remaining adjacent records can be merged

        if (rec < 1) return true; // there is no rec above to merge with because rec == 0

        uint16_t rec2 = rec;
        uint16_t rec1 = rec - 1;

        if ((ptable[rec1].src == ptable[rec2].src)
            && (ptable[rec1].offset + ptable[rec1].len == ptable[rec2].offset)) {
            
            ptable[rec1].len += ptable[rec2].len;

            if (rec2 != last_record) {
                if (!_doc_shift_rec_up(rec2 + 1, 1)) return false;

            } else {
                ptable[rec2] = (ptable_record_t) {
                    .src = PTABLE_SRC_INVALID,
                    .offset = 0,
                    .len = 0,
                };
                last_record -= 1;
            }
        }
    }

    return true;
}

/////////
// Copy Paste

void doc_set_copy_start(void) {
    doc_copy1 = core->cursor;
    vp_set_copy_start();

    if (doc_copy1 > doc_copy2) {
        doc_copy2 = 0xFFFFFFFF;
        vp_clear_copy_end();
    }
}

void doc_set_copy_end(void) {
    doc_copy2 = core->cursor;
    vp_set_copy_end();

    if (doc_copy2 < doc_copy1) {
        doc_copy1 = 0xFFFFFFFF;
        vp_clear_copy_start();
    }
}

bool _doc_copy_recs_one(record_offset_t c1, record_offset_t c2) {

    copy_ptable[0] = (ptable_record_t) {
        .src = ptable[c1.record].src,
        .offset = ptable[c1.record].offset + c1.offset,
        .len = c2.offset - c1.offset,
    };

    return true;
}

bool _doc_copy_recs_multiple(record_offset_t c1, record_offset_t c2, uint16_t num_recs) {

    // copy first record:
    copy_ptable[0] = (ptable_record_t) {
        .src = ptable[c1.record].src,
        .offset = ptable[c1.record].offset + c1.offset,
        .len = ptable[c1.record].len - c1.offset,
    };

    // copy all the middle records:
    uint16_t cptable_idx = 1;
    while (cptable_idx < num_recs - 1) {
        uint16_t ptable_idx = cptable_idx + c1.record;

        copy_ptable[cptable_idx] = (ptable_record_t) {
            .src = ptable[ptable_idx].src,
            .offset = ptable[ptable_idx].offset,
            .len = ptable[ptable_idx].len,
        };

        cptable_idx += 1;
    }


    // copy the last record:
    copy_ptable[num_recs - 1] = (ptable_record_t) {
        .src = ptable[c2.record].src,
        .offset = ptable[c2.record].offset,
        .len = c2.offset,
    };

    return true;
}

bool doc_paste(void) {

    // === Safety Checks
    // IF paste postion is between C1 and C2 - FAIL
    if (core->cursor <= doc_copy2 && core->cursor > doc_copy1) {
        buzz_play_library(2);
        return false;
    }
    // IF doc_copy1 or doc_copy2 are invalid - FAIL
    if (doc_copy1 == 0xFFFFFFFF || doc_copy2 == 0xFFFFFFFF) {
        buzz_play_library(2);
        return false;
    }

    if (doc_copy2 <= doc_copy1) {
        buzz_play_library(2);
        vp_clear_copy_end();
        vp_clear_copy_start();
        doc_copy1 = 0xFFFFFFFF;
        doc_copy2 = 0xFFFFFFFF;
        return false;
    }

    // === Copy the records from the copy selection into the Copy Piece Table
    uint32_t copy_len = doc_copy2 - doc_copy1;

    record_offset_t c1 = _doc_get_rec_offset(doc_copy1);
    record_offset_t c2 = _doc_get_rec_offset(doc_copy2);

    uint16_t num_recs = c2.record - c1.record + 1;
    if (num_recs > COPY_PTABLE_NUM_RECORDS) {
        debug_deposit("UEDT-CRIT-Cannot copy-paste, Copy selection spans too many records:", num_recs, DBG_U32_DEC);
        return false;
    }
    if ((num_recs + 1) > (PTABLE_NUM_RECORDS - last_record)) {
        debug_deposit("UEDT-CRIT-Cannot copy-paste, no room in Piece Table:", num_recs, DBG_U32_DEC);
        return false;
    }

    bool copy_fail = false;
    if (num_recs == 1) {
        copy_fail = !_doc_copy_recs_one(c1, c2);
    } else {
        copy_fail = !_doc_copy_recs_multiple(c1, c2, num_recs);
    }

    if (copy_fail) {
        debug_deposit("UEDT-CRIT-Copying failed when copying entries to Copy Piece Table", 0, DBG_NULL_VAR);
        return false;
    }

    // === Now we paste the temp_ptable records into the Piece Table
    if (core->cursor == 0) {
        if (!_doc_shift_rec_down(0, num_recs)) return false;

        if (!_doc_paste_recs(0, num_recs, copy_len)) return false;

        vp_calc_all_rows();
        vp_render_rows();
        core->cursor += copy_len;
        vp_render_vcursor();
        // for (uint32_t i = 0; i < copy_len; i++) {
        //     doc_cursor_increase();
        // }
        return true;
    }

    record_offset_t paste = _doc_get_rec_offset(core->cursor);
    if (paste.record == PTABLE_NUM_RECORDS) return false;

    if (paste.offset < ptable[paste.record].len) { // we are INSIDE a record

        if (!_doc_split_record_add(paste.record, paste.offset)) return false;
        uint16_t new_rec = paste.record + 1;

        if (!_doc_shift_rec_down(new_rec, num_recs - 1)) return false;

        if (!_doc_paste_recs(new_rec, num_recs, copy_len)) return false;

        vp_calc_all_rows();
        vp_render_rows();
        // for (uint32_t i = 0; i < copy_len; i++) {
        //     doc_cursor_increase();
        // }
        core->cursor += copy_len;
        vp_render_vcursor();
        return true;

    } else if (paste.offset == ptable[paste.record].len) { // we are at the END of a record

        uint16_t new_rec = paste.record + 1;

        if (!_doc_shift_rec_down(new_rec, num_recs)) return false;

        if (!_doc_paste_recs(new_rec, num_recs, copy_len)) return false;

        vp_calc_all_rows();
        vp_render_rows();

        //TODO: Test doing cursor += copy_len, then vp_render_vcursor()
        //      more efficient, but might break. TEST IT!!!
        // for (uint32_t i = 0; i < copy_len; i++) { 
        //     doc_cursor_increase();
        // }
        core->cursor += copy_len;
        vp_render_vcursor();
        return true;
    }

    return false;
}

bool _doc_paste_recs(uint16_t rec, uint16_t num_recs, uint32_t copy_len) {

    memcpy(ptable + rec, copy_ptable, sizeof(ptable_record_t) * num_recs);

    core->file_size += copy_len;

    return true;
}


/////////
// File navigation

void doc_cursor_increase(void) {
    if (core->cursor < core->file_size) {
        core->cursor += 1;
        vp_render_vcursor();
    }
}

void doc_cursor_decrease(void) {
    if (core->cursor > 0) {
        core->cursor -= 1;
        vp_render_vcursor();
    }
    // printf("cursor: %u\n", core->cursor);
}

void doc_cursor_top(void) {
    core->cursor = 0;
    core->vp_top_doc = 0;

    vp_calc_all_rows();
    vp_render_rows();
    vp_render_vcursor();
}

void doc_cursor_end_word(void) {

    while (core->cursor < core->file_size) {
        core->cursor += 1;
        vp_render_vcursor();
        
        if (vp_get_vcursor_cell() == ' ') break;
    }
}

void doc_cursor_start_word(void) {

    while (core->cursor > 0) {
        core->cursor -= 1;
        vp_render_vcursor();

        if (vp_get_vcursor_cell() == ' ') break;
    }

}



//////////
// Stat Calculations

static bool _doc_is_space(uint8_t symbol) {
    if (symbol == '\n' || symbol == '\t' || symbol == ' ' || symbol == '\r') {
        return true;
    } else {
        return false;
    }
}

uint32_t doc_calc_word_count(void) {
    uint32_t word_count = 0;
    bool in_word = false;

    uint32_t file_offset = 0;
    char buf[16] = {0};

    while (file_offset < core->file_size) {

        uint32_t chunk_size = (core->file_size - file_offset > 16) ? 16 : (core->file_size - file_offset);

        if (!doc_read_file(file_offset, buf, chunk_size)) {
            return 0xFFFFFFFF;
        }

        for (uint8_t i = 0; i < chunk_size; i++) {

            if (!_doc_is_space(buf[i])) {

                if (!in_word) {
                    word_count += 1;
                    in_word = true;
                }
            } else {
                in_word = false;
            }
        }

        file_offset += chunk_size;
    }

    return word_count;
}

uint8_t doc_calc_mem_usage(void) {
    return (uint8_t)( (ADD_tail * 100) / ADD_BUFFER_SIZE);
}

uint8_t doc_calc_ptable_usage(void) {
    return (uint8_t)( (last_record * 100) / PTABLE_NUM_RECORDS);
}

uint32_t doc_calc_para_count(void) {

    if (core->file_size == 0) return 0;

    uint32_t para_count = 1;
    uint32_t nl_count = 0;

    uint32_t file_offset = 0;
    char buf[16] = {0};

    while (file_offset < core->file_size) {

        uint32_t chunk_size = (core->file_size - file_offset > 16) ? 16 : (core->file_size - file_offset);

        if (!doc_read_file(file_offset, buf, chunk_size)) {
            return 0xFFFFFFFF;
        }

        for (uint8_t i = 0; i < chunk_size; i++) {

            if (buf[i] == '\r') continue;

            if (buf[i] == '\n') {
                nl_count += 1;
                if (nl_count == 2) para_count += 1;
            
            } else {
                nl_count = 0;
            }
        }

        file_offset += chunk_size;
    }

    return para_count;
}