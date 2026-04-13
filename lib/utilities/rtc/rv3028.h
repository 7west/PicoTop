#pragma once


bool rv_init(void);

bool rv_get_time(struct tm *now);

bool rv_set_time(const struct tm *now);

bool rv_PORF_check(void);

void rv_set_dst(bool is_dst);

bool rv_get_dst(void);

uint8_t _rv_read_byte(uint8_t addr);

bool _rv_write_byte(uint8_t addr, uint8_t data);

bool _rv_write_stream(uint8_t *buf, uint8_t len);

bool _rv_read_stream(uint8_t addr, uint8_t * buf, uint8_t len);

void _rv_setting_eeprom(void);

uint8_t _rv_bcd2bin(uint8_t val);

uint8_t _rv_bin2bcd(uint8_t val);