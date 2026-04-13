#include "pico/stdlib.h"

#include "batt.h"
#include "hardware/adc.h"
#include "buzz.h"


#define GPIO_BATT_ADC 28
#define GPIO_CHG_DONE 2
#define GPIO_LBO_CHG 3

static bool adc_started = false;
static bool lbo_last_state = false;

void batt_init_monitor(void) {
    adc_init();
    adc_gpio_init(GPIO_BATT_ADC);
    adc_select_input(2);

    gpio_init(GPIO_CHG_DONE);
    gpio_set_dir(GPIO_CHG_DONE, GPIO_IN);
    gpio_pull_up(GPIO_CHG_DONE);

    gpio_init(GPIO_LBO_CHG);
    gpio_set_dir(GPIO_LBO_CHG, GPIO_IN);
    gpio_pull_up(GPIO_LBO_CHG);

    adc_started = true;
}

uint16_t get_batt_voltage(void) {
    if (!adc_started) return 0;

    uint16_t counts = adc_read();
    uint32_t voltage = 0;

    for (uint8_t i = 0; i < 10; i++) {
        uint16_t counts = adc_read();
        // special bitwise formula to convert counts into voltage
        //      its V * 100
        voltage += (165 * counts) >> 10;
    }

    return (uint16_t)(voltage / 10);
}

bool batt_lbo_state(void) {

    bool lbo_state = !gpio_get(GPIO_LBO_CHG);

    if (lbo_last_state != lbo_state) {
        if (lbo_state) buzz_play_library(3);
        lbo_last_state = lbo_state;
    }

    return lbo_state;
}

bool batt_chg_done_state(void) {
    return !gpio_get(GPIO_CHG_DONE);
}

