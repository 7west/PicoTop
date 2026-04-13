#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <time.h>

#include "os_debug.h"
#include "rv3028.h"


#define RV_ADDR 0x52
#define RV_I2C_INST i2c1
#define GPIO_SDA 6
#define GPIO_SCL 7

#define RV_TIME_REG 0x00
#define RV_ID_REG 0x28
#define RV_SEC_REG 0x00
#define RV_STAT_REG 0x0E
#define RV_EEPROM_BK_REG 0x37
#define RV_CTRL1_REG 0x0F
#define RV_EECMD_REG 0x27
#define RV_EEADDR_REG 0x25
#define RV_EEDATA_REG 0x26
#define RV_RAM1_REG 0x1F

#define RV_EEPROM_CLKOUT_REG 0x35

#define RV_EEPROM_BSM_MASK 0x0C
#define RV_EEPROM_EN_BSM 0x0C

#define RV_CTRL1_DIS_EERD 0x08

#define RV_STAT_EEBUSY_MASK 0x80

#define RV_EECMD_START 0x00
#define RV_EECMD_ALL_TO_EEPROM 0x11
#define RV_EECMD_ONE_TO_EEPROM 0x21

#define RV_EEPROM_CLKOUT_DIS 0x00



static uint8_t buffer[8] = {0x00};

bool rv_init(void) {
    i2c_init(RV_I2C_INST, 100*1000);

    gpio_set_function(GPIO_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GPIO_SCL, GPIO_FUNC_I2C);

    buffer[0] = RV_ID_REG;

    if (!_rv_read_stream(RV_ID_REG, buffer, 1)) { // should return 0x44
        debug_deposit("RVC7-CRIT-RV-3028-C7 unreachable over I2C", 0, DBG_NULL_VAR);
        return false;
    }
    
    // Enable correct Backup Switchover Mode for VBAT
    _rv_setting_eeprom();

    debug_deposit("RVC7-INFO-RV-3028-C7 chip successfully setup", 0, DBG_NULL_VAR);
    return true;
}



bool rv_get_time(struct tm *now) {

    if (!_rv_read_stream(RV_TIME_REG, buffer, 7)) return false;

    now->tm_sec = _rv_bcd2bin(buffer[0]);
    now->tm_min = _rv_bcd2bin(buffer[1]);
    now->tm_hour = _rv_bcd2bin(buffer[2]);
    // drop buffer[3] because we don't care about weekday
    now->tm_mday = _rv_bcd2bin(buffer[4]);
    now->tm_mon = _rv_bcd2bin(buffer[5]) - 1;
    now->tm_year = _rv_bcd2bin(buffer[6]) + 100;

    return true;
}


bool rv_set_time(const struct tm *now) {

    buffer[0] = RV_TIME_REG;
    buffer[1] = _rv_bin2bcd(now->tm_sec);
    buffer[2] = _rv_bin2bcd(now->tm_min);
    buffer[3] = _rv_bin2bcd(now->tm_hour);
    buffer[4] = 0x00; // we don't care about the weekday
    buffer[5] = _rv_bin2bcd(now->tm_mday);
    buffer[6] = _rv_bin2bcd(now->tm_mon + 1);
    buffer[7] = _rv_bin2bcd(now->tm_year - 100);

    if (!_rv_write_stream(buffer, 8))  {
        return false;
    }

    // Write 0's only to set PORF to 0 so chip knows time was set
    _rv_write_byte(RV_STAT_REG, 0x00);

    return true;
}



// returns true if a voltage drop was detected
bool rv_PORF_check(void) {
    uint8_t stat_reg = _rv_read_byte(RV_STAT_REG);

    return (stat_reg & 0x01);
}

// setting BSM and disabling CLKOUT
void _rv_setting_eeprom(void) {
    uint8_t eeprom_bk = _rv_read_byte(RV_EEPROM_BK_REG);

    if ((eeprom_bk & RV_EEPROM_BSM_MASK) == RV_EEPROM_EN_BSM) {
        printf("BSM already set correctly\n");
        return;
    }

    // printf("Setting BSM and CLKOUT to EEPROM\n");

    // 1. Disable EEPROM Memory Refresh
    _rv_write_byte(RV_CTRL1_REG, RV_CTRL1_DIS_EERD);

    // Wait for EEPROM memory to not be busy
    uint64_t start_time = time_us_64();
    bool not_timedout = true;
    while ((_rv_read_byte(RV_STAT_REG) & RV_STAT_EEBUSY_MASK) && not_timedout) {
        not_timedout = time_us_64() - start_time < 150000;
    }
    if (!not_timedout) {
        debug_deposit("RVC7-CRIT-EEPROM busy timeout", 0, DBG_NULL_VAR);
        _rv_write_byte(RV_CTRL1_REG, 0x00);
        return;
    }

    // Ensure BSM is disabled in the RAM mirror BEFORE the EEPROM write
    _rv_write_byte(RV_EEPROM_BK_REG, eeprom_bk & ~RV_EEPROM_BSM_MASK); 

    // 2. Write BSM directly to the EEPROM cell (bypassing RAM mirror limits)
    _rv_write_byte(RV_EEADDR_REG, RV_EEPROM_BK_REG);                    // EEADDR = 0x37
    _rv_write_byte(RV_EEDATA_REG, eeprom_bk | RV_EEPROM_EN_BSM);        // EEDATA = new value
    _rv_write_byte(RV_EECMD_REG, RV_EECMD_START);                       // 0x00 Pre-command
    _rv_write_byte(RV_EECMD_REG, RV_EECMD_ONE_TO_EEPROM);               // 0x21 WRITE TO ONE EEPROM BYTE

    // Wait for EEPROM memory to not be busy
    start_time = time_us_64();
    not_timedout = true;
    while ((_rv_read_byte(RV_STAT_REG) & RV_STAT_EEBUSY_MASK) && not_timedout) {
        not_timedout = time_us_64() - start_time < 150000;
    }
    if (!not_timedout) {
        debug_deposit("RVC7-CRIT-setting BSM failed", 0, DBG_NULL_VAR);
        _rv_write_byte(RV_CTRL1_REG, 0x00);
        return;
    }

    // 3. Write CLKOUT directly to the EEPROM cell
    _rv_write_byte(RV_EEADDR_REG, RV_EEPROM_CLKOUT_REG);                  // EEADDR = 0x35
    _rv_write_byte(RV_EEDATA_REG, RV_EEPROM_CLKOUT_DIS);                  // EEDATA = new value
    _rv_write_byte(RV_EECMD_REG, RV_EECMD_START);                         // 0x00 Pre-command
    _rv_write_byte(RV_EECMD_REG, RV_EECMD_ONE_TO_EEPROM);                 // 0x21 WRITE TO ONE EEPROM BYTE

    // Wait for EEPROM memory to not be busy
    start_time = time_us_64();
    not_timedout = true;
    while ((_rv_read_byte(RV_STAT_REG) & RV_STAT_EEBUSY_MASK) && not_timedout) {
        not_timedout = time_us_64() - start_time < 150000;
    }
    if (!not_timedout) {
        debug_deposit("RVC7-CRIT-setting BSM failed", 0, DBG_NULL_VAR);
        _rv_write_byte(RV_CTRL1_REG, 0x00);
        return;
    }

    // 4. Update the RAM mirrors so changes take effect immediately without a reset
    _rv_write_byte(RV_EEPROM_BK_REG, eeprom_bk | RV_EEPROM_EN_BSM);
    _rv_write_byte(RV_EEPROM_CLKOUT_REG, RV_EEPROM_CLKOUT_DIS);

    // 5. Re-enable EEPROM Memory Refresh
    _rv_write_byte(RV_CTRL1_REG, 0x00);

    printf("BSM and CLKOUT set! :D\n");
}

uint8_t _rv_read_byte(uint8_t addr) {
    uint8_t buf = addr;

    i2c_write_blocking(RV_I2C_INST, RV_ADDR, &buf, 1, true);
    i2c_read_blocking(RV_I2C_INST, RV_ADDR, &buf, 1, false);

    return buf;
}

bool _rv_write_byte(uint8_t addr, uint8_t data) {

    uint8_t buf[2] = {addr, data};
    if (i2c_write_blocking(RV_I2C_INST, RV_ADDR, buf, 2, false) != 2) return false;

    return true;
}

// FIRST BYTE MUST BE ADDRESS, so length has to be +1 of what you want to write
bool _rv_write_stream(uint8_t * buf, uint8_t len) {
    
    if (i2c_write_blocking(RV_I2C_INST, RV_ADDR, buf, len, true) != len) return false;

    return true;
}

bool _rv_read_stream(uint8_t addr, uint8_t * buf, uint8_t len) {

    if (len > 0) buf[0] = addr;

    if (i2c_write_blocking(RV_I2C_INST, RV_ADDR, buf, 1, true) != 1) return false;
    if (i2c_read_blocking(RV_I2C_INST, RV_ADDR, buf, len, false) != len) return false;

    return true;
}

uint8_t _rv_bcd2bin(uint8_t val) {
    return val - 6 * (val >> 4);
}

uint8_t _rv_bin2bcd(uint8_t val) {
    return val + 6 * (val / 10);
}

void rv_set_dst(bool is_dst) {
    _rv_write_byte(RV_RAM1_REG, (uint8_t) is_dst);
}

bool rv_get_dst(void) {
    return (_rv_read_byte(RV_RAM1_REG) == 0x01);
}