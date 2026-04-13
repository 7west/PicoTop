#pragma once

// runs on core1 and detects and processes USB keyboard inputs into a buffer
//  for core0 to read from
// Hands off characters and special buttons. This library tries to limit all processing done
//  by users.

#include <stdio.h>
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "tusb.h"

// MODIFIER BITS
#define KBD_CTRL_BIT    0b00000001
#define KBD_SHIFT_BIT   0b00000010
#define KBD_ALT_BIT     0b00000100
#define KBD_GUI_BIT     0b00001000

// BAD ASCII (BASCII) CHARS
//  These are just for this OS and are not real
//  ascii characters, thus they are "bad"
#define KBD_BASCII_INSERT 0x95
#define KBD_BASCII_HOME 0x91
#define KBD_BASCII_END 0x93
#define KBD_BASCII_PAGE_DOWN 0x92
#define KBD_BASCII_PAGE_UP 0x94
#define KBD_BASCII_ARROW_RIGHT 0x9C
#define KBD_BASCII_ARROW_LEFT 0x9D
#define KBD_BASCII_ARROW_DOWN 0x9E
#define KBD_BASCII_ARROW_UP 0x9F
#define KBD_BASCII_F1 0x81
#define KBD_BASCII_F2 0x82
#define KBD_BASCII_F3 0x83
#define KBD_BASCII_F4 0x84
#define KBD_BASCII_F5 0x85
#define KBD_BASCII_F6 0x86
#define KBD_BASCII_F7 0x87
#define KBD_BASCII_F8 0x88
#define KBD_BASCII_F9 0x89
#define KBD_BASCII_F10 0x8A
#define KBD_BASCII_F11 0x8B
#define KBD_BASCII_F12 0x8C

// ASCII CHARS
//  So we don't have to keep referencing the ASCII table and for readability
//  However, we keep "BASCII" name for consistency
#define KBD_BASCII_DELETE 0x7F
#define KBD_BASCII_BACKSPACE 0x08
#define KBD_BASCII_NEW_LINE 0x0A
#define KBD_BASCII_ESCAPE 0x1B

typedef struct {
    uint8_t modifiers;
    uint8_t keycode;
} key_event_t;


// initializes USB Host for HID keyboard
void keyboard_init(void);

// tuh_task() wrapper
void keyboard_loop_call(void);

// pushes a key event, called from core1
bool keyboard_queue_push(const key_event_t * key);

// pops a key event, called from core0
bool keyboard_queue_pop(key_event_t* out);

bool keyboard_is_up(void);

/**
 * @brief Called by USB HID 'report received' callback
 * 
 * Filters actions based on caps lock. Sends key to _enqueue_keycode(). Also starts key repeat alarm
 * 
 * @param report 
 */
static void _process_kbd_report(hid_keyboard_report_t const *report);

// checks if given report has given keycode in it
//  useful to see if old report has a keycode in it
static inline bool _report_has_key(const hid_keyboard_report_t *report, uint8_t keycode);

// finds the first non-zero keycode in the given report
//  used to find backup 'repeat' key
static inline uint8_t _first_held_key(const hid_keyboard_report_t *report);

/**
 * @brief uses keyboard_queue_push() to queue keys. Handles shift/caps lock as well
 * 
 * @param keycode mostly ASCII keycodes (special keys have bad ASCII codes)
 * @param modifier left-right modifier agnostic
 */
static void _enqueue_keycode(uint8_t keycode, uint8_t modifier);

// called by alarm to enqueue keycode, if still held down
static int64_t _repeat_cb(alarm_id_t id, void *user_data);

// starts repeat alarm. Filters out special keys, those never repeat when held down
static void _arm_repeat(uint8_t keycode);

// cancels all repeat alarms and keycode
static void _stop_repeat(void);

// checks if a keycode is special. see definition for list
static bool _key_is_special(uint8_t keycode);

// converts HID keycode into bad ASCII (bascii) code
static uint8_t _key_special_output(uint8_t keycode);

// toggles capslock state and keyboard LED
static void _key_check_capslock(void);

