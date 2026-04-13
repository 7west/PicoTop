#pragma once

#include <stdint.h>
#include <stdbool.h>

// must be a power of 2
#define DEBUG_MAX_ENTRIES 128


typedef enum {
    DBG_NULL_VAR,
    DBG_U32_DEC,
    DBG_U32_HEX,
    DBG_U8_HEX,
} os_debug_type_t;

typedef struct{
    const char *msg;
    uint32_t var;
    os_debug_type_t type;
} os_debug_entry_t;


/**
 * @brief Format "XXXX-XXXX-message" Library/Source-CRIT/WARN/INFO-message
 * 
 * @param msg MAX OF 80 CHARS!!! WITH THE VAR PRINTED!!!
 * @param var variable you might want to include with log entry
 * @param type var type, to aid with printing
 */
void debug_deposit(const char * msg, uint32_t var, os_debug_type_t type);


/**
 * @brief returns total number of entries
 * 
 * @return uint8_t MAX 128 (DEBUG_MAX_ENTRIES)
 */
uint32_t debug_get_total_entries();


/**
 * @brief returns oldest log entry (somehow...)
 * 
 * @param abs_idx the absolute index out of total log entries (max 4 billion)
 * @param out MUST BE 81 CHARS
 */
void debug_withdraw_entry(uint32_t abs_idx, char *out);


// runs periodically to write debug log outputs to a text log file
void debug_file_periodic(void);


void debug_print_log_UART(void);