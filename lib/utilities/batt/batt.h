#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

void batt_init_monitor(void);

// returns voltage * 100
//  runs 10 samples and takes an average
uint16_t get_batt_voltage(void);

// 1 = Low Battery
bool batt_lbo_state(void);

// 1 = Charging done
bool batt_chg_done_state(void);