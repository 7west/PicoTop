// uedit_doc handles the Document Model for uEdit:
//      BASE and ADD records in Piece Table
//      holding current copy of file's byte stream

#pragma once

#include "uedit.h"

#define PTABLE_NUM_RECORDS 2048 // byte size is this * 12 = 24,576 (HOLY CRAP!)
#define COPY_PTABLE_NUM_RECORDS 512 // byte size = 12 * 512 = 6144 B
#define ADD_BUFFER_SIZE (1024 * 32) // 32,786 B

// Piece Table Record
typedef struct {
    uint32_t src;
    uint32_t offset;
    uint32_t len;
} ptable_record_t;

typedef struct {
    uint32_t offset;
    uint16_t record;
} record_offset_t;

typedef struct {
    ptable_record_t ptable_mem[PTABLE_NUM_RECORDS];
    ptable_record_t copy_ptable_mem[COPY_PTABLE_NUM_RECORDS];
    uint8_t ADD_buffer_mem[ADD_BUFFER_SIZE];
} uedit_doc_t;



// initializes Document Model
void doc_init(uedit_core_t *c, uedit_doc_t *d);

// resets piece table, used after saving file
void doc_reset_ptable(void);

/////////
// Reading Piece Table

/**
 * @brief Called for bytes of data from document model, indiscriminate of data source (ADD vs BASE)
 * 
 * @param offset offset in file
 * @param buf buffer to fill
 * @param len duh
 */
bool doc_read_file(uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * @brief Writes requested bytes to buf. Handles BASE vs ADD records
 * 
 * @param record index of record in piece table
 * @param internal_offset offset INSIDE that record
 * @param buf buffer this function will fill for caller
 * @param len duh
 */
static bool _doc_transfer_bytes(uint16_t record, uint32_t internal_offset, uint8_t *buf, uint32_t len);

// prints entire file out on UART. Useful sometimes
void doc_stream_file_UART(void);



/////////
// Record Manipulation - Adding

// Adds a character (symbol) at current core->cursor position
bool doc_add_char(uint8_t symbol);

// shifts given "rec" down "shift_n" entries. Shifts all records after it down as well
static bool _doc_shift_rec_down(uint16_t rec, uint16_t shift_n);

// splits record into og_rec and og_rec2 leaving an empty record between
//      the two at rec + 1
static bool _doc_split_record_add(uint16_t rec, uint32_t rec_offset);

// takes current core->cursor position
static uint16_t _doc_get_record(uint32_t pos);

/**
 * @brief Fills a newly created record with necessary details
 * 
 * @param new_rec index of new record
 * @param symbol actual uint8_t value going there
 * @return false - piece table is full
 */
static bool _doc_fill_new_record(uint16_t new_rec, uint8_t symbol);

// declared in uedit_doc.c:
//record_offset_t _doc_get_rec_offset(uint32_t pos);


/////////
// Record Manipulation - Removing

/**
 * @brief Handles backspace key by user
 * 
 * removes char before cursor and moves cursor back one
 */
bool doc_bksp_char(void);

/**
 * @brief Handles delete key by user
 * 
 * Removes char at cursor and does not move cursor
 */
bool doc_del_char(void);

// removes the character given at position "pos"
static bool _doc_rem_char(uint32_t pos);

// shifts record at "rec" (and all records below it) up "shift_n" number of records
static bool _doc_shift_rec_up(uint16_t rec, uint16_t shift_n);

// this will split the current record at rec_offset and remove the char at rec_offset from
//      the piece table
static bool _doc_split_record_rem(uint16_t rec, uint32_t rec_offset);

// if a record has len == 0 due to character removal, the record is deleted
//      if this leaves two adjacent records next to each other, they are merged
static bool _doc_trim_record(uint16_t rec);


/////////
// Copy Paste

void doc_set_copy_start(void);

void doc_set_copy_end(void);

bool _doc_copy_recs_one(record_offset_t c1, record_offset_t c2);

bool _doc_copy_recs_multiple(record_offset_t c1, record_offset_t c2, uint16_t num_recs);

bool doc_paste(void);

bool _doc_paste_recs(uint16_t rec, uint16_t num_recs, uint32_t copy_len);


/////////
// File navigation

// called by arrow key right, increases cursor position by one and calls vp_render_vcursor()
void doc_cursor_increase(void);

// called by arrow key left, decreases cursor position by one and calls vp_render_vcursor()
void doc_cursor_decrease(void);

// moves cursor to top of file and rerenders screen
void doc_cursor_top(void);

// Moves cursor to the right until it is over a ' ' (space) character
void doc_cursor_end_word(void);

// Moves cursor to the left until it is over a ' ' (space) character
void doc_cursor_start_word(void);


//////////
// Stat Calculations

uint32_t doc_calc_word_count(void);

uint8_t doc_calc_mem_usage(void);

uint8_t doc_calc_ptable_usage(void);

uint32_t doc_calc_para_count(void);

static bool _doc_is_space(uint8_t symbol);