#include "includes.h"

static time_t last_min_epoch = 0;

#define INIT_USELESS_DELAY 100
#define LIGHTING_SYMBOL 174
#define PLUG_SYMBOL 175
#define GPIO_USER_8 8
#define GPIO_USER_9 9


void draw_batt_symbol(void) {

    if (batt_lbo_state()) {
        term_draw_char(62, 0, LIGHTING_SYMBOL);
    } else if (batt_chg_done_state()) {
        term_draw_char(62, 0, PLUG_SYMBOL);
    } else {
        term_draw_char(62, 0, 0); // blank
    }
    term_invert_cell(62, 0);
}

void init_os(void) {
    sleep_ms(2000);
    term_clear_screen();

    gpio_init(GPIO_USER_8);
    gpio_init(GPIO_USER_9);

    term_draw_string(1, 0, "7West");
    term_invert_line(0);
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    term_set_cursor(0,1);
    term_puts("Starting up WestOS...");
    term_new_line();
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    batt_init_monitor();

    term_puts("Starting up RTC... ");
    sleep_ms(INIT_USELESS_DELAY); // this is stupid
    if (rtc_init()) {
        term_puts("Success");
    } else {
        debug_deposit("RTCL-WARN-RTC failed to start up at OS startup", 0, DBG_NULL_VAR);
        term_puts("Fail");
    }
    // get initial time for periodic minute check
    // rtc_get_time(&now);
    // last_min = now.tm_min;
    // next_check_us = time_us_64() + ((59ULL - now.tm_sec) * 1000000ULL);

    update_ui_time();
    term_new_line();
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    term_puts("Mounting FAT16 & virtual file system... ");
    sleep_ms(INIT_USELESS_DELAY); // this is stupid
    if (vfs_init()) {
        term_puts("Success");
        vault_update_status();
    } else {
        debug_deposit("VFSL-WARN-Virtual File System failed to start up at OS startup", 0, DBG_NULL_VAR);
        term_puts("Fail");
    }
    term_new_line();
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    term_puts("Detecting keyboard... ");
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    uint64_t start_time = time_us_64();
    bool not_timeout = true;
    while (not_timeout && !keyboard_is_up()) {
        not_timeout = time_us_64() - start_time < 2000000;
    }
    if (not_timeout) {
        term_puts("Success");
    } else {
        debug_deposit("USBK-WARN-USB Keyboard failed to be detected. Replug in?", 0, DBG_NULL_VAR);
        term_puts("Fail");
    }
    term_new_line();
    sleep_ms(INIT_USELESS_DELAY); // this is stupid

    term_new_line();
    term_puts("OS Loaded");
    // buzz_play_library(5); // TODO: disabled for now because I reboot A LOT
    sleep_ms(1000); // not stupid

    prog_switch(&shell_prog);
    draw_ui_bar();

}

// for functions needed every minute
void check_periodic_min(void) {
    
    struct timespec ts;    
    rtc_get_epoch(&ts);

    if ((ts.tv_sec / 60) != last_min_epoch) {
        // === Stuff that runs every minute:

        update_ui_time();

        draw_batt_symbol();

        // debug_file_periodic();






        last_min_epoch = ts.tv_sec / 60;
    }
}