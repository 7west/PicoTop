

#include "usb_kbd.h"
#include <string.h>
#include "os_debug.h"


#include "tusb_config.h"

//TODO
//  Testing

#define MAX_REPORT  4
#define MAX_KEY_QUEUE_SIZE 64

// hold-down => repeat, durations
#define KEY_INITIAL_DELAY_MS 500
#define KEY_REPEAT_PERIOD_MS 50

// length of HID keycodes of certain type (e.g. a-z, F1-F12)
#define KEY_HID_ALPHA_LENGTH (0x1D - 0x04)
#define KEY_HID_SPECIAL_LENGTH (0x52 - 0x3A)
#define KEY_HID_FKEY_LENGTH (0x45 - 0x3A)

static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

// Each HID instance can has multiple reports
static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];


static key_event_t key_q[MAX_KEY_QUEUE_SIZE];
static volatile uint8_t head = 0;
static volatile uint8_t tail = 0;


void keyboard_init(void) {
    tuh_init(BOARD_TUH_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    printf("USB START\n");

}

void keyboard_loop_call(void) {
    tuh_task();
}

bool keyboard_queue_push(const key_event_t * key) {
    uint8_t h = head;
    uint8_t next = (h + 1) & (MAX_KEY_QUEUE_SIZE - 1);
    if (next == tail) {
        debug_deposit("USBK-WARN-Keyboard buffer overflow/full", 0, DBG_NULL_VAR);
        return false; //overflow
    }

    key_q[h] = *key; // write data VERIFY THIS IS RIGHT!!!
    __dmb(); // data memory barrier
    head = next; // publish
    return true;
}

bool keyboard_queue_pop(key_event_t* out) {
    uint8_t t = tail;
    if (t == head) return false; //empty

    *out = key_q[t];
    __dmb();
    tail = (t + 1) & (MAX_KEY_QUEUE_SIZE - 1);
    return true;
}

static uint8_t keyboard_dev_addr;
static uint8_t keyboard_instance;
static uint8_t leds = 0;
static bool keyboard_up = false;

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    // printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        printf("USB Keyboard plugged in\n");
        debug_deposit("USBK-INFO-USB HID Keyboard device plugged in", 0, DBG_NULL_VAR);
        keyboard_dev_addr = dev_addr;
        keyboard_instance = instance;
        keyboard_up = true;
    }

    // printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

    // By default host stack will use activate boot protocol on supported interface.
    // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
    if ( itf_protocol == HID_ITF_PROTOCOL_NONE ) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        // printf("HID has %u reports \r\n", hid_info[instance].report_count);
    }

    // request to receive report
    // tuh_hid_report_received_cb() will be invoked when report is available
    if ( !tuh_hid_receive_report(dev_addr, instance) ) {
        // printf("Error: cannot request to receive report\r\n");
        debug_deposit("USBK-CRIT-Cannot request to receive reports. idk what this means.", 0, DBG_NULL_VAR);
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    debug_deposit("USBK-INFO-USB HID device unplugged/unmounted", 0, DBG_NULL_VAR);
    keyboard_up = false;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
        // TU_LOG2("HID receive boot keyboard report\r\n");
        _process_kbd_report( (hid_keyboard_report_t const*) report );
        break;

        // case HID_ITF_PROTOCOL_MOUSE:
        //   TU_LOG2("HID receive boot mouse report\r\n");
        //   process_mouse_report( (hid_mouse_report_t const*) report );
        // break;

        default:
        // Generic report requires matching ReportID and contents with previous parsed report info
        // process_generic_report(dev_addr, instance, report, len);
        debug_deposit("USBK-WARN-Non-keyboard USB device plugged in and it issued a report. Ignoring.", 0, DBG_NULL_VAR);
        break;
    }

    // continue to request to receive report
    if ( !tuh_hid_receive_report(dev_addr, instance) ) {
        // printf("Error: cannot request to receive report\r\n");
        debug_deposit("USBK-CRIT-Cannot request to receive reports. idk what this means.", 0, DBG_NULL_VAR);
    }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

static hid_keyboard_report_t prev_report = { 0, 0, {0} };
static hid_keyboard_report_t cur_report = { 0, 0, {0} };

static alarm_id_t repeat_alarm = -1;
static uint8_t repeat_keycode = 0;

static bool caps_lock = false;

static inline bool _report_has_key(const hid_keyboard_report_t *report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

static inline uint8_t _first_held_key(const hid_keyboard_report_t *report) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i]) {
            return report->keycode[i];
        }
    }
    return 0;
}

static void _enqueue_keycode(uint8_t keycode, uint8_t modifier) {
    key_event_t key;
    key.keycode = 0;
    // compress modifiers (we don't care about right vs left)
    key.modifiers = ((modifier >> 4) | modifier ) & 0x0F;

    if (_key_is_special(keycode)) {
        key.keycode = _key_special_output(keycode);

    } else {
        bool is_shift = modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        // checks if keycode is A-Z (0x04 - 0x1D)
        if (caps_lock && ((uint8_t)(keycode - 0x04) <= (KEY_HID_ALPHA_LENGTH))) {
            is_shift = true;
        }
        // keycode2ascii is a massive provided lookup table
        key.keycode = keycode2ascii[keycode][is_shift ? 1 : 0];
        if (key.keycode == 0x0D) key.keycode = 0x0A;
    }
    
    // only queue if keycode != 0
    // ALL keys, besides chars and our list of specials, will be dropped
    if (key.keycode) {
        keyboard_queue_push(&key);
    }
}

static int64_t _repeat_cb(alarm_id_t id, void *user_data) {
    (void) id;
    (void) user_data;

    uint8_t keycode = repeat_keycode;
    if (!keycode) return 0;

    // still held? if not:
    if (!_report_has_key(&cur_report, keycode)) {
        repeat_keycode = 0;
        repeat_alarm = -1;
        return 0; //stop repeating
    }

    _enqueue_keycode(keycode, cur_report.modifier);
    // returns are the time until the next alarm trigger [us]
    return (int64_t) KEY_REPEAT_PERIOD_MS * 1000; // reschedule
}

static void _arm_repeat(uint8_t keycode) {
    // filter out special keys
    if (_key_is_special(keycode)) {
        if ((keycode >= HID_KEY_ARROW_RIGHT && keycode <= HID_KEY_ARROW_UP) 
                || keycode == HID_KEY_DELETE
                || keycode == HID_KEY_PAGE_UP
                || keycode == HID_KEY_PAGE_DOWN) {
            // do nothing
            // don't block repeat for arrow keys, delete, or PgUp/PgDn
        } else {
            _stop_repeat();
            return;
        }
    }

    repeat_keycode = keycode;

    // repeat_alarm id is <=0 if alarm is real, is -1 if alarm does not exist
    if (repeat_alarm >= 0) cancel_alarm(repeat_alarm);
    repeat_alarm = add_alarm_in_ms((uint32_t) KEY_INITIAL_DELAY_MS, _repeat_cb, NULL, true);
}

static void _stop_repeat(void) {
    repeat_keycode = 0;
    if (repeat_alarm >= 0) {
        cancel_alarm(repeat_alarm);
        repeat_alarm = -1;
    }
}


static void _process_kbd_report(hid_keyboard_report_t const *report) {
    cur_report = *report;

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];

        // skip if keycode is == 0
        if (!keycode) continue;

        // printf("keycode: %02X = \n", keycode);

        if (!_report_has_key(&prev_report, keycode)) {

            if (keycode != HID_KEY_CAPS_LOCK) {
                _enqueue_keycode(keycode, report->modifier);

                _arm_repeat(keycode);
            } else {
                _key_check_capslock();
            }
            
        }

    }

    // if repeat key was released, stop repeat, or pick the "first held key"
    if (repeat_keycode && !_report_has_key(report, repeat_keycode)) {
        uint8_t next_keycode = _first_held_key(report);

        if (next_keycode) {
            _arm_repeat(next_keycode);
        } else {
            _stop_repeat();
        }
    } else if (!_first_held_key(report)) {
        _stop_repeat();
    }

    prev_report = *report;
}

static bool _key_is_special(uint8_t keycode) {
    bool is_special = false;

    // F1-F12, INSERT, HOME, PAGE UP, PAGE DOWN, DELETE, END, 
    // ARROWS (RIGHT, LEFT, DOWN, UP) = 0x3A to 0x52
    if ((uint8_t)(keycode - 0x3A) <= (KEY_HID_SPECIAL_LENGTH)) {
        is_special = true;
        // technically this catches print screen, scroll lock,
        //  and pause, but the next function will just not use it
    }

    return is_special;
}

static uint8_t _key_special_output(uint8_t keycode) {
    uint8_t bascii = 0;

    if ((uint8_t)(keycode - 0x3A) <= (KEY_HID_FKEY_LENGTH)) {
        bascii = keycode + (KBD_BASCII_F1 - HID_KEY_F1);
        return bascii;
    }

    switch (keycode) {
        case HID_KEY_INSERT:
            bascii = KBD_BASCII_INSERT;
            break;
        case HID_KEY_HOME:
            bascii = KBD_BASCII_HOME;
            break;
        case HID_KEY_END:
            bascii = KBD_BASCII_END;
            break;
        case HID_KEY_PAGE_DOWN:
            bascii = KBD_BASCII_PAGE_DOWN;
            break;
        case HID_KEY_PAGE_UP:
            bascii = KBD_BASCII_PAGE_UP;
            break;
        case HID_KEY_ARROW_RIGHT:
            bascii = KBD_BASCII_ARROW_RIGHT;
            break;
        case HID_KEY_ARROW_LEFT:
            bascii = KBD_BASCII_ARROW_LEFT;
            break;
        case HID_KEY_ARROW_DOWN:
            bascii = KBD_BASCII_ARROW_DOWN;   
            break;
        case HID_KEY_ARROW_UP:
            bascii = KBD_BASCII_ARROW_UP;
            break;
        case HID_KEY_DELETE:
            bascii = 0x7F; // this is actually ASCII
                // but isn't in keycode2ascii[][] for some reason
            break;
    }

    return bascii;
}

static void _key_check_capslock(void) {
    if (caps_lock) {
        caps_lock = false;
        leds = 0; 
    } else {
        caps_lock = true;
        leds = KEYBOARD_LED_CAPSLOCK;
    }
    tuh_hid_set_report(keyboard_dev_addr, keyboard_instance, 0, HID_REPORT_TYPE_OUTPUT, &leds, sizeof(leds));
}

bool keyboard_is_up(void) {
    return keyboard_up;
}