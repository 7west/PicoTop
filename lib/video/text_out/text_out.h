#pragma once
// Draws characters from font.h into the frame_buffer
// frame_buffer is broken into 8x16 pixel cells

#include <stdio.h>
#include "pico/stdlib.h"
#include "video_out.h"

#define TEXT_FONT_WIDTH 8
#define TEXT_FONT_HEIGHT 16
#define TEXT_NUM_COLS (VIDEO_H_RES / TEXT_FONT_WIDTH)
#define TEXT_NUM_ROWS (VIDEO_V_RES / TEXT_FONT_HEIGHT)


void text_init(void);

/**
 * @brief Draws single character at cell location
 * 
 * @param x_cell max = TEXT_NUM_COLS
 * @param y_cell max = TEXT_NUM_ROWS
 * @param symbol the character you want printed
 * @param invert_flag inverts pixels' color
 */
void text_draw_char(uint16_t x_cell, uint16_t y_cell, uint8_t symbol, bool invert_flag);

/**
 * @brief Draws a string at the cell location. DOES NOT WRAP
 * 
 * @param x_cell max = TEXT_NUM_COLS
 * @param y_cell max = TEXT_NUM_ROWS
 * @param text_string do not go past line end
 */
void text_draw_string(uint16_t x_cell, uint16_t y_cell, const char * text_string, uint16_t len);

/**
 * @brief Writes 0x00 (black) to entire frame buffer
 * 
 */
void text_clear_screen(void);

/**
 * @brief Inverts the pixels' color of a single cell
 * 
 * @param x_cell max = TEXT_NUM_COLS
 * @param y_cell max = TEXT_NUM_ROWS
 */
void text_invert_cell(uint16_t x_cell, uint16_t y_cell);

/**
 * @brief turns all black-space (non character pixels) gray
 * 
 * @param x_cell 
 * @param y_cell 
 */
void text_gray_cell(uint16_t x_cell, uint16_t y_cell);

/**
 * @brief Undoes "text_gray_cell". Turns gray to black.
 * 
 * @param x_cell 
 * @param y_cell 
 */
void text_restore_cell(uint16_t x_cell, uint16_t y_cell);

/**
 * @brief Moves a 16px row up/down and wipes the src row
 * 
 * @param line_num the line of characters to be moved
 * @param down_flag true = down, false = up
 */
void text_move_line(uint16_t line_num, bool down_flag);



static uint16_t _xcell_to_pixels(uint16_t x_cell);

static uint16_t _ycell_to_pixels(uint16_t y_cell);