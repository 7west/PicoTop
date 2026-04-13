#pragma once

// SD Block interface, uses SPI

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"


#define SD_BLOCK_SIZE 0x200 //512

typedef struct {
    uint8_t r1;
    uint8_t ext[4];
    uint8_t len;
} sd_resp_t;


/**
 * @brief runs at startup to initialize SD Card
 * 
 * @return true successful init;
 * @return false failed to init, dump debug log
 */
void sd_block_boot_init(void);

/**
 * @brief run by sd_block_boot_init and can be run by user from shell to swap SDs
 * 
 * @return true successful SD init;
 * @return false failed to init, dump debug log
 */
bool sd_block_soft_init(void);

/**
 * @brief Returns SD data to supplied buffer. Runs multiple 'single block reads'.
 * 
 * @param lba [SECTOR] address of first read, not byte address
 * @param count number of sectors to read
 * @param dst buffer supplied that the data is written to, assumes buffer is correct size
 * @return true read was successful;
 * @return false failed to read, dump debug log
 */
bool sd_read_sectors(uint32_t lba, uint32_t count, uint8_t *dst);

/**
 * @brief Writes SD data from supplied buffer. Runs multiple 'single block writes'.
 * 
 * @param lba [SECTOR] address of first write, not byte address
 * @param count number of sectors to write
 * @param src buffer supplied that the data is written from, assumes buffer size is correct
 * @return true write was successful;
 * @return false failed to write, dump debug log
 */
bool sd_write_sectors(uint32_t lba, uint32_t count, uint8_t *src);

/**
 * @brief Gets the sector (or block) size
 * 
 * @return uint32_t almost always 0x200
 */
uint32_t sd_get_sector_size(void);

/**
 * @brief Gets the number of sectors. Multiply with sd_get_sector_size to get SD capacity in bytes
 * 
 * @return uint32_t value retrieved at sd_block_soft_init
 */
uint32_t sd_get_sector_count(void);

// get tail of SD to leave blank when formatting. Calculated in sd_block_soft_init()
uint32_t sd_get_tail_sectors(void);

/**
 * @brief disconnects the SD card for unplugging
 * 
 * @return true SD safe to unplug;
 * @return false N/A never returns false
 */
bool sd_block_deinit(void);



// writes just one sector of SD
static bool _sd_write_one_sector(uint32_t lba, uint8_t *src);

/**
 * @brief Sends one command and receives response. sets CS.
 * 
 * @param cmd SD_CMD_##
 * @param arg 4 bytes of arguments for the SD command 
 * @param resp response struct to store r1
 */
static void _sd_cmd_resp(uint8_t cmd, uint8_t *arg, sd_resp_t * resp);

/**
 * @brief Sends one command and reads data sent back. Sets CS. Drops CRC bytes.
 * 
 * @param cmd SD_CMD_##
 * @param arg 4 bytes of arguments for the SD command 
 * @param buf buffer where data from SD card will be stored
 * @param len expected length of buffer
 * @return true data read successful;
 * @return false error occured, dump debug log
 */
static bool _sd_cmd_data(uint8_t cmd, uint8_t *arg, uint8_t *buf, uint16_t len);

// takes uint32_t parameters for a command and loads them into a 4 byte array
static void _sd_fill_cmd_arg(uint32_t params, uint8_t *arg);

// waits for byte of 'data response' token format: 0bxxx0xxx1
static uint8_t _sd_wait_for_data_response(void);

// wait for byte of R1 token format: 0b0xxxxxxx
static uint8_t _sd_wait_for_r1(void);

// parse 'data response' token. False = bad response, dump debug log
static bool _sd_parse_data_response(uint8_t drt);

// sends one command. does NOT control CS. Use in other functions
static void _sd_cmd_arg_send(uint8_t cmd, uint8_t *arg);

// fails if R1 returns error. handles idle/not idle case with 2nd param
static bool _sd_parse_r1(uint8_t r1, bool idle_expected);

// set CS high or low. If high, clock 8 times
static void _sd_chip_select(bool state);

// reads/writes one byte on SPI
static uint8_t _sd_spi_transfer(uint8_t write);

// computes CRC7 for commands. Only really used at startup. Uses sd_crc7_table
static uint8_t _sd_compute_crc7(const uint8_t *data, uint8_t len);

// parses bits from CSD register
static uint32_t _sd_get_csd_bits(uint8_t * buf, uint16_t high, uint16_t low);