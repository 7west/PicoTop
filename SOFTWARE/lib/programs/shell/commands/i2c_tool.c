#include "../commands.h"
#include "hardware/i2c.h"

#define MAX_WRITE_BYTES 16

// i2ct -p PERIPH_ADDR -a REG_ADDR -r num_bytes
// i2ct -p PERIPH_ADDR -a REG_ADDR -w num_bytes 0x01 0x02 0x03

bool cmd_i2c_tool_help(const out_t *out) {
    out->write(out->ctx, "I2C tool using broken out pins on PicoTop\ni2ct -p [PERIPH ADDR] -a [REG ADDR] [-r/-w] [READ/WRITE & NUM BYTES] 0x01...\nPeripheral address, register address, and read/write and number of bytes\nfollowed by bytes in hexadecimal, if write was selected\n Examples:\n i2ct -p 0x52 -a 0x01 -r 4\n i2ct -p 0x52 -a 0x02 -w 2 0x34 0x21");
    return true;
}

// user used i2ct command wrong
bool cmd_i2c_tool_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_i2c_tool_help(out);
}

// some deeper I2C issue
bool cmd_i2c_fail(const out_t *out) {
    out->write(out->ctx, "I2C had a failure");
    return true;
}

uint8_t argv_to_hex(const char *arg) {
    // optional "0x" prefix
    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X'))
        arg += 2;

    uint8_t result = 0;
    for (uint8_t i = 0; i < 2 && arg[i]; i++) {
        char c = arg[i];
        uint8_t nibble;
        if (c >= '0' && c <= '9') {
            nibble = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            nibble = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            nibble = c - 'A' + 10;
        } else {
            break; // invalid character
        }
        result = (result << 4) | nibble;
    }
    return result;
}

uint8_t argv_to_dec(const char *arg) {
    uint16_t result = 0;
    for (int i = 0; i < 3 && arg[i]; i++) {
        char c = arg[i];
        if (c >= '0' && c <= '9') {
            result = result * 10 + (c - '0');
        } else {
            break; // invalid character
        }
    }
    return (uint8_t)(result > 255 ? 255 : result);
}

// i2ct -p PERIPH_ADDR -a REG_ADDR -r num_bytes
// i2ct -p PERIPH_ADDR -a REG_ADDR -w num_bytes 0x01 0x02 0x03

bool cmd_i2c_tool(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_i2c_tool_help(out);
    }

    if (argc < 7 || argc > 9 + MAX_WRITE_BYTES) { 
        return cmd_i2c_tool_fail(out);
    }

    bool read_flag = false;
    bool write_flag = false;
    bool periph_flag = false;
    bool reg_flag = false;
    uint8_t periph_addr = 0x00;
    uint8_t reg_addr = 0x00;
    uint8_t num_bytes = 0;
    uint8_t byte_buf[MAX_WRITE_BYTES + 1] = {0};

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "p:a:r:w:", &state)) != -1) {

        switch(c){
            case 'p':
                periph_flag = true;
                periph_addr = argv_to_hex(state.optarg);
                break;

            case 'a':
                reg_flag = true;
                reg_addr = argv_to_hex(state.optarg);
                break;

            case 'r':
                read_flag = true;
                num_bytes = argv_to_dec(state.optarg);
                break;

            case 'w':
                write_flag = true;
                num_bytes = argv_to_dec(state.optarg);
                break;

            default:
                return cmd_i2c_tool_fail(out);
        }
    }

    // cant have read and write
    if (write_flag && read_flag) return cmd_i2c_tool_fail(out);
    // must have addresses and read or write
    if (!(periph_flag && reg_flag && (write_flag || read_flag))) return cmd_i2c_tool_fail(out);
    // max read/write size
    if (num_bytes > MAX_WRITE_BYTES || num_bytes == 0) return cmd_i2c_tool_fail(out);
    // check bytes provided for -w
    if (write_flag && (argc - state.optind != num_bytes)) return cmd_i2c_tool_fail(out);



    if (write_flag) {
        byte_buf[0] = reg_addr;
        for (uint8_t i = 0; i < num_bytes; i++) {
            byte_buf[i + 1] = argv_to_hex(argv[i + state.optind]);
        }

        if (i2c_write_blocking(i2c1, periph_addr, byte_buf, num_bytes + 1, false) != num_bytes + 1) {
            return cmd_i2c_fail(out);
        }
    }

    if (read_flag) {

        byte_buf[0] = reg_addr;

        if (i2c_write_blocking(i2c1, periph_addr, byte_buf, 1, true) != 1) {
            return cmd_i2c_fail(out);
        }
        if (i2c_read_blocking(i2c1, periph_addr, byte_buf, num_bytes, false) != num_bytes) {
            return cmd_i2c_fail(out);
        }

        char hex[6];
        for (uint8_t i = 0; i < num_bytes; i++) {
            snprintf(hex, 6, "0x%02X ", byte_buf[i]);
            out->write(out->ctx, hex);
        }
    }

    return true;
}