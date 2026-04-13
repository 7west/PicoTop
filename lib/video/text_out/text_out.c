
#include <string.h>

#include "text_out.h"
#include "frame_buffer.h"
#include "font.h"
// #include "font_cyan.h"
#include "os_debug.h"

#define TEXT_ROW_SIZE (VIDEO_H_RES * TEXT_FONT_HEIGHT)
#define TEXT_COLOR_NEG 0x00
#define TEXT_COLOR_POS 0xFF
#define TEXT_COLOR_GRAY 0x49

void text_init(void) {
    video_out_setup();
}

void text_draw_char(uint16_t x_cell, uint16_t y_cell, uint8_t symbol, bool invert_flag) {
    uint16_t x = _xcell_to_pixels(x_cell);
    uint16_t y = _ycell_to_pixels(y_cell);

    if (x >= VIDEO_H_RES || y >= VIDEO_V_RES) { 
        debug_deposit("TXTO-WARN:text_draw_char() out of range", 0, DBG_NULL_VAR);
        return;
    }

    const uint8_t *src_char = &font8x16[symbol][0];

    for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {

        uint8_t *dest = &framebuf[x + ((y + iy) * VIDEO_H_RES)];
        const uint8_t *src = &src_char[iy * TEXT_FONT_WIDTH];

        if (invert_flag) {
            for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
                *dest++ = ~(*src++);
            }
        } else {
            for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
                *dest++ = *src++;
            }
        }     
    }
}

// void text_draw_char(uint16_t x_cell, uint16_t y_cell, uint8_t symbol, bool invert_flag) {

//     uint16_t x = _xcell_to_pixels(x_cell);
//     uint16_t y = _ycell_to_pixels(y_cell);

//     if (x >= VIDEO_H_RES || y >= VIDEO_V_RES) {

//         debug_deposit("TXTO-WARN:text_draw_char() out of range", 0, DBG_NULL_VAR);
//         return;
//     }

//     for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {

//         for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
//             uint32_t pixel_addr = (x + ix) + ((y + iy) * VIDEO_H_RES);
//             uint8_t pixel_data = font8x16[symbol][ix + (iy * TEXT_FONT_WIDTH)];

//             if (invert_flag) pixel_data = ~pixel_data;

//             framebuf[pixel_addr] = pixel_data;
//         }        
//     }
// }

void text_draw_string(uint16_t x_cell, uint16_t y_cell, const char * text_string, uint16_t len) {

    uint16_t x = _xcell_to_pixels(x_cell);
    uint16_t y = _ycell_to_pixels(y_cell);

    if (y_cell + ((x_cell + len - 1) / TEXT_NUM_COLS) >= TEXT_NUM_ROWS)  {
        debug_deposit("TXTO-WARN:text_draw_string() out of range", 0, DBG_NULL_VAR);
        return;
    }

    for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {

        uint8_t *dest_row = &framebuf[x + ((y + iy) * VIDEO_H_RES)];

        for (uint16_t is = 0; is < len; is++) {

            uint8_t symbol = text_string[is];
            // find start of font row once per row
            const uint8_t *src_row = &font8x16[symbol][iy * TEXT_FONT_WIDTH];

            memcpy(&dest_row[is * TEXT_FONT_WIDTH], src_row, TEXT_FONT_WIDTH);

        }
    }
}


void text_clear_screen(void) {
    memset(framebuf, TEXT_COLOR_NEG , VIDEO_H_RES * VIDEO_V_RES);
}


void text_invert_cell(uint16_t x_cell, uint16_t y_cell) {
    uint16_t x = _xcell_to_pixels(x_cell);
    uint16_t y = _ycell_to_pixels(y_cell);

    if (x >= VIDEO_H_RES || y >= VIDEO_V_RES) { 
        debug_deposit("TXTO-WARN:text_invert_cell() out of range", 0, DBG_NULL_VAR);
        return;
    }

    for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {
        for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
        
            uint32_t pixel_addr = (x + ix) + ((y + iy) * VIDEO_H_RES);

            framebuf[pixel_addr] = ~(framebuf[pixel_addr]);

        }        
    }
}

void text_gray_cell(uint16_t x_cell, uint16_t y_cell) {
    uint16_t x = _xcell_to_pixels(x_cell);
    uint16_t y = _ycell_to_pixels(y_cell);

    if (x >= VIDEO_H_RES || y >= VIDEO_V_RES) { 
        debug_deposit("TXTO-WARN:text_gray_cell() out of range", 0, DBG_NULL_VAR);
        return; 
    }

    for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {
        for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
        
            uint32_t pixel_addr = (x + ix) + ((y + iy) * VIDEO_H_RES);

            if (framebuf[pixel_addr] == TEXT_COLOR_NEG) {
                framebuf[pixel_addr] = TEXT_COLOR_GRAY;
            }
        }        
    }
}

void text_restore_cell(uint16_t x_cell, uint16_t y_cell) {
    uint16_t x = _xcell_to_pixels(x_cell);
    uint16_t y = _ycell_to_pixels(y_cell);

    if (x >= VIDEO_H_RES || y >= VIDEO_V_RES) { 
        debug_deposit("TXTO-WARN:text_restore_cell() out of range", 0, DBG_NULL_VAR);
        return; 
    }

    for (uint16_t iy = 0; iy < TEXT_FONT_HEIGHT; iy++) {
        for (uint16_t ix = 0; ix < TEXT_FONT_WIDTH; ix++) {
        
            uint32_t pixel_addr = (x + ix) + ((y + iy) * VIDEO_H_RES);

            if (framebuf[pixel_addr] == TEXT_COLOR_GRAY) {
                framebuf[pixel_addr] = TEXT_COLOR_NEG;
            }
        }        
    }
}

void text_move_line(uint16_t line_num, bool down_flag) {
    uint32_t start = line_num * TEXT_ROW_SIZE;

    if (down_flag) { // moving line down
        if (line_num == 29) {
            debug_deposit("TXTO-WARN:text_move_line() out of range", 0, DBG_NULL_VAR);
            return;
        }

        memmove(framebuf + start + TEXT_ROW_SIZE,
                framebuf + start,
                TEXT_ROW_SIZE);
        
        memset(framebuf + start, TEXT_COLOR_NEG, TEXT_ROW_SIZE);
    } else {
        if (line_num == 0)  {
            debug_deposit("TXTO-WARN:text_move_line() out of range", 0, DBG_NULL_VAR);   
            return;
        }

        memmove(framebuf + start - TEXT_ROW_SIZE,
                framebuf + start,
                TEXT_ROW_SIZE);
        
        memset(framebuf + start, TEXT_COLOR_NEG, TEXT_ROW_SIZE);
    }
}

static uint16_t _xcell_to_pixels(uint16_t x_cell) {
    return x_cell * TEXT_FONT_WIDTH;
}

static uint16_t _ycell_to_pixels(uint16_t y_cell) {
    return y_cell * TEXT_FONT_HEIGHT;
}