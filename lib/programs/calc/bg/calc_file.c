#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "calc_file.h"
#include "calc_ui.h"
#include "vfs.h"
#include "usb_kbd.h"

// valid chars for FAT16 8.3 file names
static bool is_valid_83_char(char c);
// reads one 'line', uses 16 byte buffer
static bool read_line(file_desc_t fd, uint8_t *dst, uint32_t file_size);





bool calc_save_file(calc_cell_t *cells, calc_state_t state) {
    char file_name[VFS_FILE_NAME_LEN] = {0x00};

    key_event_t key;
    key.keycode = 0x00;
    uint8_t key_count = 0;

    // writes prompt to bottom UI row
    calc_save_prompt();

    // takes user's keyboard input
    while (1) {
        if (keyboard_queue_pop(&key)) {
            if (key.keycode == '\n') break;

            if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == 'c') {
                calc_save_output("File save cancel");
                return true;
            }

            if (is_valid_83_char(key.keycode) && key_count < VFS_FILE_SIZE_LEN-1) {
               
                file_name[key_count] = key.keycode;
                calc_save_add_char(key_count, key.keycode);
                key_count += 1;
            }

            if (key.keycode == KBD_BASCII_BACKSPACE && key_count > 0) {
                key_count -= 1;
                file_name[key_count] = '\0';
                calc_save_add_char(key_count, 0x00);
            }

        }
    }

    // calc file extension
    file_name[key_count++] = '.';
    file_name[key_count++] = 'C';
    file_name[key_count++] = 'A';
    file_name[key_count] = '\0';

    printf("file name: %s\n", file_name);

    file_desc_t fd = vfs_open(file_name, FS_MODE_CREATE | FS_MODE_WRITE);
    if (fd < 1) {
        calc_save_output("File save failed. Invalid name?");
        return true;
    }
    calc_save_output("Saving...");

    for (uint16_t i = 0; i < state.cell_count; i++) {

        if (cells[i].flags & CALC_CELL_FLAG_OCCUPIED) {
            vfs_write(fd, cells[i].expr, cells[i].len);
            vfs_write(fd, "\n=", 2);
            vfs_write(fd, cells[i].out_msg, strlen(cells[i].out_msg));
            vfs_write(fd, "\n", 1);
        }

    }

    vfs_close(fd);

    calc_save_output("Save Successful");
}

bool calc_load_file(calc_cell_t *cells, calc_state_t *state) {

    char file_name[VFS_FILE_NAME_LEN] = {0x00};
    key_event_t key;
    key.keycode = 0x00;
    uint8_t key_count = 0;

    calc_load_prompt();

    // takes user keyboard input
    while (1) {
        if (keyboard_queue_pop(&key)) {
            if (key.keycode == '\n') break;

            if ((key.modifiers & KBD_CTRL_BIT) && key.keycode == 'c') {
                calc_save_output("File open cancel");
                return true;
            }

            if (is_valid_83_char(key.keycode) && key_count < VFS_FILE_SIZE_LEN-1) {
               
                file_name[key_count] = key.keycode;
                calc_save_add_char(key_count, key.keycode);
                key_count += 1;
            }

            if (key.keycode == KBD_BASCII_BACKSPACE && key_count > 0) {
                key_count -= 1;
                file_name[key_count] = '\0';
                calc_save_add_char(key_count, 0x00);
            }

        }
    }

    file_name[key_count++] = '.';
    file_name[key_count++] = 'C';
    file_name[key_count++] = 'A';
    file_name[key_count] = '\0';

    file_desc_t fd = vfs_open(file_name, FS_MODE_READ_ONLY);
    if (fd < 1) {
        calc_save_output("File open failed. Invalid name?");
        return true;
    }
    calc_save_output("Loading...");

    uint32_t file_size = vfs_get_file_size(fd);
    state->cell_count = 0;

    memset(cells, 0x00, sizeof(calc_cell_t) * CALC_NUM_CELLS);
    for (uint16_t i = 0; i < CALC_NUM_CELLS; i++) {
        memset(cells[i].expr, ' ', CALC_EXPR_TOT_LEN);
    }

    for (uint16_t i = 0; i < CALC_NUM_CELLS; i++) {
        if (!read_line(fd, cells[i].expr, file_size)) break;
        cells[i].flags = CALC_CELL_FLAG_EXISTS | CALC_CELL_FLAG_OCCUPIED;
        state->cell_count += 1;
        if (!read_line(fd, NULL, file_size)) break;
    }
    state->curr_cell = 0;
    state->top_ui_cell = 0;

    vfs_close(fd);

    calc_save_output("Load Successful");
}


static bool read_line(file_desc_t fd, uint8_t *dst, uint32_t file_size) {
    uint32_t pos = vfs_tell(fd);
    if (pos >= file_size) return false;

    uint8_t buf[16];
    uint32_t written = 0;

    while (pos < file_size) {
        uint32_t remaining = file_size - pos;
        uint32_t to_read = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
        vfs_read(fd, buf, to_read);

        for (uint32_t j = 0; j < to_read; j++) {
            if (buf[j] == '\n') {
                pos += j + 1;
                goto done;
            }
            if (dst && written < CALC_EXPR_TOT_LEN)
                dst[written++] = buf[j];
        }
        pos += to_read;
    }

done:
    if (dst)
        memset(dst + written, ' ', CALC_EXPR_TOT_LEN - written);
    vfs_seek(fd, pos, FS_SEEK_START);
    return true;
}


static bool is_valid_83_char(char c) {
    if (c < 0x20 || c >= 0x7F)           return false;
    if (strchr("\"*+,/:;<=>?[\\]|.", c))  return false;
    return true;
}