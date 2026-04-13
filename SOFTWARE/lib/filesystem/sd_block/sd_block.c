
#include <string.h>

#include "sd_block.h"
#include "os_debug.h"

#define printf(...) ((void)0)

#define SD_SPI_DUMMY 0xFF
#define SD_SPI_START_BYTE 0xFE
#define SD_CHIP_HIGH true
#define SD_CHIP_LOW false

#define SD_SPI_PORT spi0

// Adafruit Fruit Jam
// #define SD_SPI_PIN_CD 33
// #define SD_SPI_PIN_SCK 34
// #define SD_SPI_PIN_TX 35
// #define SD_SPI_PIN_RX 36
// #define SD_SPI_PIN_CS 39

// PicoTop Board
#define SD_SPI_PIN_CD 24
#define SD_SPI_PIN_SCK 22
#define SD_SPI_PIN_TX 23
#define SD_SPI_PIN_RX 20
#define SD_SPI_PIN_CS 21

// SD SPI COMMANDS
#define SD_CMD_0 0 // Reset
#define SD_CMD_8 8 // Interface Condition
#define SD_CMD_9 9 // Read CSD (Card Specific Data)
#define SD_CMD_10 10 // Read CID (Card ID Reg)
#define SD_CMD_12 12 // Stop transmission
#define SD_CMD_16 16 // Set Block Length
#define SD_CMD_17 17 // Read single block
#define SD_CMD_18 18 // Read multiple blocks
#define SD_CMD_24 24 // Write block
#define SD_CMD_25 25 // Write multiple blocks
#define SD_ACMD_41 41// send operation condition
#define SD_ACMD_51 51 // Read (SCR) SD Config Register
#define SD_CMD_55 55 // next is App CMD
#define SD_CMD_58 58 // Read OCR

// R1 STATUSES
#define SD_R1_IDLE_GOOD 0x01
#define SD_R1_READY_GOOD 0x00
#define SD_IDLE_EXPECTED true
#define SD_IDLE_NOT_EXPECTED false

// TIMEOUTS
//  TODO: finetune these????
#define SD_WRITE_TIMEOUT_US 100000
#define SD_READ_TIMEOUT_US 1000000
#define SD_ACMD41_TIMEOUT_US (1000 * 1000)
#define SD_R1_TIMEOUT_US 1000
#define SD_DATA_READ_TIMEOUT_US 100000
#define SD_DEINIT_TIMEOUT_US 100000


static const uint8_t sd_crc7_table[256];
static uint32_t card_cap_bytes;
static uint32_t card_cap_sectors;
static uint32_t tail_size_bytes; // reserved empty space after FAT16 volume


void sd_block_boot_init(void) {
    
    gpio_init(SD_SPI_PIN_CD);
    gpio_pull_up(SD_SPI_PIN_CD);
    gpio_set_dir(SD_SPI_PIN_CD, GPIO_IN);
    
    gpio_init(SD_SPI_PIN_CS);
    gpio_set_dir(SD_SPI_PIN_CS, GPIO_OUT);
    gpio_put(SD_SPI_PIN_CS, SD_CHIP_HIGH);

    gpio_set_function(SD_SPI_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_SPI_PIN_TX, GPIO_FUNC_SPI);
    gpio_set_function(SD_SPI_PIN_RX, GPIO_FUNC_SPI);

    // limit to 100kHz for setup
    spi_init(SD_SPI_PORT, 100 * 1000);
    spi_set_format(SD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // many start up actions may be called by OS again later if
    //  another SD is plugged in:
    printf("starting soft init\n");
}

bool sd_block_soft_init(void) {
    sd_resp_t resp;

    // because we may call this after boot if we swap SD cards
    //  and init requires <400kHz
    spi_set_baudrate(SD_SPI_PORT, 100 * 1000);

    gpio_put(SD_SPI_PIN_CS, 1);

    // if chip is not detected, return failure
    // if (!gpio_get(SD_SPI_PIN_CD)) { 
    if (gpio_get(SD_SPI_PIN_CD)) { 
        debug_deposit("SDBK-CRIT-SD Chip Detect not detected. SD init halted.", 0, DBG_NULL_VAR);
        printf("Chip detect failed\n");
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    //After detecting, you must wait 1ms for voltage to stabilize
    //  MAYBE REMOVE THIS, because it might already by 1ms by now.
    sleep_ms(2);

    printf("sending dummy bytes\n");
    for (uint8_t i = 0; i < 10; i++) {
        _sd_spi_transfer(SD_SPI_DUMMY);
    }

    uint8_t arg[4];
    
    // CMD Reset 1:
    //  sent to eat 0x3F that sometimes ruins R1 detection logic
    //  so we also have to pad some extra dummy bytes
    memset(arg, 0x00, 4);
    _sd_cmd_resp(SD_CMD_0, arg, &resp);
    _sd_spi_transfer(SD_SPI_DUMMY);
    _sd_spi_transfer(SD_SPI_DUMMY);
    printf("Real startup now:\n");

    memset(arg, 0x00, 4);
    _sd_cmd_resp(SD_CMD_0, arg, &resp);
    if (!_sd_parse_r1(resp.r1, SD_IDLE_EXPECTED)) {
        debug_deposit("SDBK-CRIT-CMD0 'Reset' R1 response failed: ", resp.r1, DBG_U8_HEX);
        return false;
    }

    // CMD interface condition
    memset(arg, 0x00, 4);
    arg[2] = 0x01;
    arg[3] = 0xAA;
    _sd_cmd_resp(SD_CMD_8, arg, &resp);
    if (!_sd_parse_r1(resp.r1, SD_IDLE_EXPECTED)) {
        debug_deposit("SDBK-CRIT-CMD8 'CMD interface' R1 response failed: ", resp.r1, DBG_U8_HEX);
        return false;
    }

    
    // Read OCR
    memset(arg, 0x00, 4);
    _sd_cmd_resp(SD_CMD_58, arg, &resp);
    if (!_sd_parse_r1(resp.r1, SD_IDLE_EXPECTED)) {
        debug_deposit("SDBK-CRIT- 1st CMD58 'read OCR' R1 response failed: ", resp.r1, DBG_U8_HEX);
        return false;
    }

    
    // set operation condition and keep trying until it is done
    uint64_t start_time = time_us_64();
    bool not_timedout = true;
    uint16_t count = 0;
    while (resp.r1 != SD_R1_READY_GOOD && not_timedout) {
        // next CMD is ACMD
        memset(arg, 0x00, 4);
        _sd_cmd_resp(SD_CMD_55, arg, &resp);


        // send operation condition
        memset(arg, 0x00, 4);
        _sd_cmd_resp(SD_ACMD_41, arg, &resp);

        not_timedout = time_us_64() - start_time < SD_ACMD41_TIMEOUT_US;
        count++;
    }
    printf("R1 = %02X\n", resp.r1);
    if (!not_timedout) {
        debug_deposit("SDBK-CRIT-ACMD41 never completed. R1 is still Idle:", resp.r1, DBG_U32_HEX);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }
    debug_deposit("SDBK-INFO-ACMD41 successful, R1 shows not idle after # attmpts: ", count, DBG_U32_DEC);
    

    // Read OCR again
    memset(arg, 0x00, 4);
    _sd_cmd_resp(SD_CMD_58, arg, &resp);
    _sd_parse_r1(resp.r1, SD_IDLE_NOT_EXPECTED);
    for (uint8_t i = 0; i < resp.len; i++) {
        printf("OCR byte 1: %02X\n", resp.ext[i]);
    }

    // Check if OCR Power up status bit is low (LOW = busy, bad)
    if (!(resp.ext[0] >> 7)) {
        debug_deposit("SDBK-CRIT-OCR Card Power up status bit is busy. Init Failed. OCR B4:", resp.ext[0], DBG_U8_HEX);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    // Check if high capacity card (bad)
    if (resp.ext[0] & 0x40) {
        debug_deposit("SDBK-CRIT-OCR High Card Capacity Bit is set. Init Failed OCR B4:", resp.ext[0], DBG_U8_HEX);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    // set block length to 512 (0x200)
    memset(arg, 0x00, 4);
    arg[2] = SD_BLOCK_SIZE >> 8;
    arg[3] = SD_BLOCK_SIZE & 0xFF;
    _sd_cmd_resp(SD_CMD_16, arg, &resp);
    _sd_parse_r1(resp.r1, SD_IDLE_NOT_EXPECTED);



    // get Card ID Register
    uint8_t buf[16];
    memset(arg, 0x00, 4);
    _sd_cmd_data(SD_CMD_10, arg, buf, 16);
    printf("CID register:\n");
    for (uint8_t i = 0; i < 16; i++) {
        printf("%02X\n", buf[i]);
    }


    // get Card Specific Data Register
    //  very necessary
    memset(arg, 0x00, 4);
    _sd_cmd_data(SD_CMD_9, arg, buf, 16);
    printf("CSD register:\n");
    for (uint8_t i = 0; i < 16; i++) {
        printf("%02X\n", buf[i]);
    }

    // set SPI bus speed to 10MHz
    //  25MHz is possible, but I don't trust the circuit board? Test?
    spi_set_baudrate(SD_SPI_PORT, 10 * 1000 * 1000);

    // Caclulate SD Card Capacity
    uint8_t read_bl_len = _sd_get_csd_bits(buf, 83, 80);
    uint8_t c_size_mult = _sd_get_csd_bits(buf, 49, 47);
    uint16_t c_size = _sd_get_csd_bits(buf, 73, 62);

    uint32_t block_len = 1 << (read_bl_len);
    uint32_t mult = 1 << (c_size_mult + 2);
    card_cap_bytes = (c_size + 1) * mult * block_len;
    card_cap_sectors = card_cap_bytes / SD_BLOCK_SIZE;

    uint8_t sector_size = _sd_get_csd_bits(buf, 45, 39);
    uint8_t wp_grp_size = _sd_get_csd_bits(buf, 38, 32);
    uint8_t write_bl_len = _sd_get_csd_bits(buf, 25, 22);

    // computes Write Protect Group Bytes (will be tail size for formatting SD)
    tail_size_bytes = (sector_size + 1) * (1 << write_bl_len) * (wp_grp_size + 1);

    debug_deposit("SDBK-INFO-SD Card Init successful. Cap [bytes]: ", card_cap_bytes, DBG_U32_DEC);
    return true;
}

uint32_t sd_get_sector_size(void) {
    return SD_BLOCK_SIZE;
}

uint32_t sd_get_sector_count(void) {
    return card_cap_sectors;
}

uint32_t sd_get_tail_sectors(void) {
    return tail_size_bytes / SD_BLOCK_SIZE;
}

bool sd_read_sectors(uint32_t lba, uint32_t count, uint8_t *dst) {

    if (gpio_get(SD_SPI_PIN_CD)) { 
        debug_deposit("SDBK-CRIT-SD Chip Detect not detected. SD read halted.", 0, DBG_NULL_VAR);
        return false;
    }

    if (lba + count > card_cap_sectors) {
        debug_deposit("SDBK-CRIT-Attempt to read out of range of SD Card. Max: ", lba + count, DBG_U32_HEX);
        return false;
    }

    uint8_t arg[4];

    for (uint32_t i = 0; i < count; i++) {

        _sd_fill_cmd_arg((lba + i) * SD_BLOCK_SIZE, arg);

        if (!_sd_cmd_data(SD_CMD_17, arg, dst + (SD_BLOCK_SIZE * i), SD_BLOCK_SIZE)) {
            debug_deposit("SDBK-CRIT-Sector read failed. Block Address: ", lba + i, DBG_U32_HEX);
            return false;
        }
    }

    return true;
}

bool sd_write_sectors(uint32_t lba, uint32_t count, uint8_t *src) {
    if (lba + count > card_cap_sectors) {
        debug_deposit("SDBK-CRIT-Attempt to write out of range of SD Card. Max: ", lba + count, DBG_U32_HEX);
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {

        if (!_sd_write_one_sector((lba + i) * SD_BLOCK_SIZE, src + (SD_BLOCK_SIZE * i))) {
            debug_deposit("SDBK-CRIT-Sector write failed. Block Address: ", lba + i, DBG_U32_HEX);
            return false;
        }
    }

    return true;
}

static bool _sd_write_one_sector(uint32_t lba, uint8_t *src) {

    if (gpio_get(SD_SPI_PIN_CD)) { 
        debug_deposit("SDBK-CRIT-SD Chip Detect not detected. SD write halted.", 0, DBG_NULL_VAR);
        return false;
    }

    _sd_chip_select(SD_CHIP_LOW);

    uint8_t arg[4];
    _sd_fill_cmd_arg(lba, arg);
    _sd_cmd_arg_send(SD_CMD_24, arg);
    uint8_t response = _sd_wait_for_r1();

    if (!_sd_parse_r1(response, SD_IDLE_NOT_EXPECTED)) {
        debug_deposit("SDBK-CRIT-Data write failed due to bad R1:", response, DBG_U8_HEX);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    // LLM said you need a 0xFF gap?
    _sd_spi_transfer(SD_SPI_DUMMY);
    _sd_spi_transfer(SD_SPI_DUMMY);
    _sd_spi_transfer(SD_SPI_START_BYTE);

    // send the actual data
    for (uint16_t i = 0; i < SD_BLOCK_SIZE; i++) {
        _sd_spi_transfer(src[i]);
    }
    // send dummy CRC
    _sd_spi_transfer(SD_SPI_DUMMY);
    _sd_spi_transfer(SD_SPI_DUMMY);

    response = _sd_wait_for_data_response();
    if (!_sd_parse_data_response(response)) {
        debug_deposit("SDBK-CRIT-Data write failed at Data Ready Token", 0, DBG_NULL_VAR);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    //wait until SD is not busy (waiting for 0xFF)
    uint64_t write_start_time = time_us_64();
    response = 0x00;
    bool not_timedout = true;

    while (response != 0xFF && not_timedout) {
        response = _sd_spi_transfer(SD_SPI_DUMMY);
        not_timedout = time_us_64() - write_start_time < SD_WRITE_TIMEOUT_US;
    }
    if (!not_timedout) {
        debug_deposit("SDBK-CRIT-SD Write timeout", 0, DBG_NULL_VAR);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }
    
    _sd_chip_select(SD_CHIP_HIGH);
    return true;
}



bool sd_block_deinit(void) {
    // for the next SD Card
    //  I know it is double-tapped
    spi_set_baudrate(SD_SPI_PORT, 100 * 1000);

    // Makes sure SD is not busy by waiting for 0xFF from SD
    _sd_chip_select(SD_CHIP_LOW);
    uint8_t response = 0x00;
    uint64_t start_time = time_us_64();
    bool not_timedout = true;
    while (response != 0xFF && not_timedout) {
        response = _sd_spi_transfer(SD_SPI_DUMMY);
        not_timedout = time_us_64() - start_time < SD_DEINIT_TIMEOUT_US;
    }
    if (!not_timedout) {
        debug_deposit("SDBK-WARN-Waiting for 0xFF timed out while deiniting, but proceeded anyway", 0, DBG_NULL_VAR);
    }
    _sd_chip_select(SD_CHIP_HIGH);

    debug_deposit("SDBK-INFO-SD Card deinitialized", 0, DBG_NULL_VAR);
    return true;
}

static bool _sd_cmd_data(uint8_t cmd, uint8_t *arg, uint8_t *buf, uint16_t len) { 
    _sd_chip_select(SD_CHIP_LOW);

    _sd_cmd_arg_send(cmd, arg);

    uint8_t response = _sd_wait_for_r1();

    // makes sure R1 is proper before reading cycling until 0xFE
    if (!_sd_parse_r1(response, SD_IDLE_NOT_EXPECTED)) {
        debug_deposit("SDBK-CRIT-Data read failed due to bad R1:", response, DBG_U8_HEX);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    uint64_t start_time = time_us_64();
    response = 0xFF;
    bool not_timedout = true;

    while ((response != 0xFE) && not_timedout) {
        response = _sd_spi_transfer(SD_SPI_DUMMY);
        not_timedout = time_us_64() - start_time < SD_READ_TIMEOUT_US;
        // Uncomment for debugging:
        // printf("SDBK-WARN-Response to CMD%d: %02X\n", cmd, response);
    }
    if (!not_timedout) {
        debug_deposit("SDBK-CRIT-Read Timeout: Never received 0xFE 'data ready' byte", 0, DBG_NULL_VAR);
        _sd_chip_select(SD_CHIP_HIGH);
        return false;
    }

    // Reads Data
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = _sd_spi_transfer(SD_SPI_DUMMY);
    }

    // Reads CRC - ignoring here, but might use in future...
    for (uint8_t i = 0; i < 2; i++) {
        _sd_spi_transfer(SD_SPI_DUMMY);
    }

    _sd_chip_select(SD_CHIP_HIGH);

    return true;
}

static bool _sd_parse_r1(uint8_t r1, bool idle_expected) {

    uint8_t expected_r1 = idle_expected ? SD_R1_IDLE_GOOD : SD_R1_READY_GOOD;

    if (r1 != expected_r1) {
        debug_deposit("SDBK-CRIT-unexpected R1 response: ", r1, DBG_U8_HEX);
        printf("SDBK-CRIT-unexpected R1 response: 0x%02X\n", r1);

        return false;
    } else {
        return true;
    }
}

static bool _sd_parse_data_response(uint8_t drt) {
    drt = drt & 0x1F;

    if (drt == 0x05) {
        return true;
    } else if (drt == 0x0B) {
        debug_deposit("SDBK-CRIT-Data Ready Token: CRC Error:", drt, DBG_U8_HEX);
        return false;
    } else if (drt == 0x0D) {
        debug_deposit("SDBK-CRIT-Data Ready Token: Write Error:", drt, DBG_U8_HEX);
        return false;
    } else {
        debug_deposit("SDBK-CRIT-Data Ready Token: Some weird ass value", drt, DBG_U8_HEX);
        return false;
    }
}


static void _sd_cmd_resp(uint8_t cmd, uint8_t *arg, sd_resp_t *resp) {

    _sd_chip_select(SD_CHIP_LOW);

    _sd_cmd_arg_send(cmd, arg);

    uint8_t response = _sd_wait_for_r1();

    resp->r1 = response;

    if (cmd == 8 || cmd == 58) {
        resp->ext[0] = _sd_spi_transfer(SD_SPI_DUMMY);
        resp->ext[1] = _sd_spi_transfer(SD_SPI_DUMMY);
        resp->ext[2] = _sd_spi_transfer(SD_SPI_DUMMY);
        resp->ext[3] = _sd_spi_transfer(SD_SPI_DUMMY);
        resp->len = 4;
    }

    _sd_chip_select(SD_CHIP_HIGH);
}

static uint8_t _sd_wait_for_r1(void) {
    uint64_t start_time = time_us_64();
    bool not_timedout = true;
    uint8_t response = 0xFF;
    // Change to time based count?
    while (((response & 0x80) == 0x80) && not_timedout) {
        response = _sd_spi_transfer(SD_SPI_DUMMY);
        // Uncomment for debugging:
        // printf("SDBK-WARN-Response to CMD%d: %02X\n", cmd, response);
        not_timedout = time_us_64() - start_time < SD_R1_TIMEOUT_US;
    }
    if (!not_timedout) {
        debug_deposit("SDBK-CRIT-Timeout of R1 token. Last token: ", response, DBG_U8_HEX);
    }
    return response;
}

static uint8_t _sd_wait_for_data_response(void) {
    uint64_t start_time = time_us_64();
    bool not_timedout = true;
    uint8_t response = 0xFF;
    // Change to time based.

    while (!((response & 0x11) == 0x01) && not_timedout) {
        response = _sd_spi_transfer(SD_SPI_DUMMY);
        not_timedout = time_us_64() - start_time < SD_DATA_READ_TIMEOUT_US;
    }
    if (!not_timedout) {
        debug_deposit("SDBK-CRIT-Timeout waiting for 'data response token'. Last byte: ", response, DBG_U8_HEX);
    }
    return response;
}

static void _sd_fill_cmd_arg(uint32_t params, uint8_t *arg) {
    arg[0] = params >> 24;
    arg[1] = (params >> 16) & 0xFF;
    arg[2] = (params >> 8) & 0xFF;
    arg[3] = params & 0xFF;
}

static void _sd_cmd_arg_send(uint8_t cmd, uint8_t *arg) {
    uint8_t payload[6] = {
        cmd | 0x40,
        arg[0],
        arg[1],
        arg[2],
        arg[3],
        0x00,
    };

    payload[5] = (_sd_compute_crc7(payload, 5) << 1) | 1;

    for (uint8_t i = 0; i < 6; i++) {
        _sd_spi_transfer(payload[i]);
    }
}

static uint8_t _sd_spi_transfer(uint8_t write) {
    uint8_t read;
    spi_write_read_blocking(SD_SPI_PORT, &write, &read, 1);
    return read;
}

void _sd_chip_select(bool state) {
    gpio_put(SD_SPI_PIN_CS, state);

    // LLM says you need a few clocks after CS high???
    if (state == SD_CHIP_HIGH) {
        uint8_t ff = SD_SPI_DUMMY;
        spi_write_blocking(SD_SPI_PORT, &ff, 1);
    }
}

static uint8_t _sd_compute_crc7(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++) {
        crc = sd_crc7_table[crc ^ data[i]];
    }

    crc >>= 1;
    return crc;
}

static uint32_t _sd_get_csd_bits(uint8_t * buf, uint16_t high, uint16_t low) {
    uint32_t result = 0;
    int len = high - low + 1;
    
    for (int i = high; i >= low; i--) {
        int byte_idx = 15 - (i / 8);
        int bit_idx = i % 8;
        if (buf[byte_idx] & (1 << bit_idx)) {
            result |= (1ULL << (i - low));
        }
    }
    return result;
}

// Precomputed because why not.
static const uint8_t sd_crc7_table[256] = {
    0x00, 0x12, 0x24, 0x36, 0x48, 0x5A, 0x6C, 0x7E,
    0x90, 0x82, 0xB4, 0xA6, 0xD8, 0xCA, 0xFC, 0xEE,
    0x32, 0x20, 0x16, 0x04, 0x7A, 0x68, 0x5E, 0x4C,
    0xA2, 0xB0, 0x86, 0x94, 0xEA, 0xF8, 0xCE, 0xDC,
    0x64, 0x76, 0x40, 0x52, 0x2C, 0x3E, 0x08, 0x1A,
    0xF4, 0xE6, 0xD0, 0xC2, 0xBC, 0xAE, 0x98, 0x8A,
    0x56, 0x44, 0x72, 0x60, 0x1E, 0x0C, 0x3A, 0x28,
    0xC6, 0xD4, 0xE2, 0xF0, 0x8E, 0x9C, 0xAA, 0xB8,
    0xC8, 0xDA, 0xEC, 0xFE, 0x80, 0x92, 0xA4, 0xB6,
    0x58, 0x4A, 0x7C, 0x6E, 0x10, 0x02, 0x34, 0x26,
    0xFA, 0xE8, 0xDE, 0xCC, 0xB2, 0xA0, 0x96, 0x84,
    0x6A, 0x78, 0x4E, 0x5C, 0x22, 0x30, 0x06, 0x14,
    0xAC, 0xBE, 0x88, 0x9A, 0xE4, 0xF6, 0xC0, 0xD2,
    0x3C, 0x2E, 0x18, 0x0A, 0x74, 0x66, 0x50, 0x42,
    0x9E, 0x8C, 0xBA, 0xA8, 0xD6, 0xC4, 0xF2, 0xE0,
    0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x70,
    0x82, 0x90, 0xA6, 0xB4, 0xCA, 0xD8, 0xEE, 0xFC,
    0x12, 0x00, 0x36, 0x24, 0x5A, 0x48, 0x7E, 0x6C,
    0xB0, 0xA2, 0x94, 0x86, 0xF8, 0xEA, 0xDC, 0xCE,
    0x20, 0x32, 0x04, 0x16, 0x68, 0x7A, 0x4C, 0x5E,
    0xE6, 0xF4, 0xC2, 0xD0, 0xAE, 0xBC, 0x8A, 0x98,
    0x76, 0x64, 0x52, 0x40, 0x3E, 0x2C, 0x1A, 0x08,
    0xD4, 0xC6, 0xF0, 0xE2, 0x9C, 0x8E, 0xB8, 0xAA,
    0x44, 0x56, 0x60, 0x72, 0x0C, 0x1E, 0x28, 0x3A,
    0x4A, 0x58, 0x6E, 0x7C, 0x02, 0x10, 0x26, 0x34,
    0xDA, 0xC8, 0xFE, 0xEC, 0x92, 0x80, 0xB6, 0xA4,
    0x78, 0x6A, 0x5C, 0x4E, 0x30, 0x22, 0x14, 0x06,
    0xE8, 0xFA, 0xCC, 0xDE, 0xA0, 0xB2, 0x84, 0x96,
    0x2E, 0x3C, 0x0A, 0x18, 0x66, 0x74, 0x42, 0x50,
    0xBE, 0xAC, 0x9A, 0x88, 0xF6, 0xE4, 0xD2, 0xC0,
    0x1C, 0x0E, 0x38, 0x2A, 0x54, 0x46, 0x70, 0x62,
    0x8C, 0x9E, 0xA8, 0xBA, 0xC4, 0xD6, 0xE0, 0xF2,
};