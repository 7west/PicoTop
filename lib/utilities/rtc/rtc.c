#include "rtc.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/aon_timer.h"
#include "hardware/powman.h"

#include "os_debug.h"
#include "rv3028.h"


static const uint8_t sakamoto_algo[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
static volatile bool rp2350_in_deep_sleep = false;
static volatile bool dst_alarm_pending;
static volatile bool is_dst;
static volatile bool time_valid;
static uint8_t mday = 0;

bool rtc_init(void) {
    powman_timer_set_1khz_tick_source_xosc();

    struct tm initial_time = {0};

    rv_init();

    // check if RV time is valid
    if (!rv_PORF_check()) {
        rv_get_time(&initial_time);

        time_valid = true;
        debug_deposit("RTCL-INFO-RTC initiated with real time from RV-3028-C7.", 0, DBG_NULL_VAR);
    } else {
        initial_time.tm_year = 2026 - 1900;
        initial_time.tm_mon = 0;
        initial_time.tm_mday = 22;
        initial_time.tm_hour = 11;
        initial_time.tm_min = 37;
        initial_time.tm_sec = 45;
        initial_time.tm_isdst = 0;

        time_valid = false;
        debug_deposit("RTCL-WARN-RTC initiated with dummy time. User must set time", 0, DBG_NULL_VAR);
    }

    // is_dst = _should_be_dst(&initial_time);

    if (!aon_timer_is_running()) {
        if(!aon_timer_start_calendar(&initial_time)) return false;
    } else {
        if(!aon_timer_set_time_calendar(&initial_time)) return false;
    }

    dst_alarm_pending = false;
    _set_dst_alarm();
    
    // if the time on RV is valid
    if (time_valid) {
        
        // get the DST state of that time
        is_dst = rv_get_dst();

        // this just forces the code to check if the RV has the wrong DST setting for the current time
        //      it is kind of hacky, but it will work
        dst_alarm_pending = true;
        rtc_get_time(&initial_time);

    }

    return true;
}

bool rtc_set_time(const struct tm *in_time, bool set_rv, bool change_dst) {;
    if (!in_time) return false;
    if (!aon_timer_is_running()) {
        debug_deposit("RTCL-WARN-Failed to set time. Timer is not running", 0, DBG_NULL_VAR);
        return false;
    }

    // only set the RV if requested by caller
    if (set_rv && !rv_set_time(in_time)) {
        return false;
    }

    if(!aon_timer_set_time_calendar(in_time)) {
        return false;
    }

    if (change_dst) {
        is_dst = _should_be_dst(in_time);

        dst_alarm_pending = false;
        _set_dst_alarm();
    }
    

    if (set_rv) rv_set_dst(is_dst);

    

    time_valid = true;
    return true;
}



bool rtc_get_time(struct tm *out_time) {
    if (!out_time) return false;

    if (!aon_timer_is_running() ) {
        return false;
    }

    if (!aon_timer_get_time_calendar(out_time)) {
        return false;
    }

    if (dst_alarm_pending) { // check and modify time based on dst
        if (is_dst != _should_be_dst(out_time)) {

            // pull most up to date time from RV chip
            rv_get_time(out_time);
            rtc_set_time(out_time, false, false);

            struct timespec ts;
            aon_timer_get_time(&ts);

            if (is_dst) { //was DST, now NOT DST, November
                printf("leaving DST\n");
                ts.tv_sec -= 3600;
                is_dst = false;
            } else { // was not DST, now DST, March
                printf("entering DST\n");
                ts.tv_sec += 3600;
                is_dst = true;
            }
            rv_set_dst(is_dst);
            aon_timer_set_time(&ts);

            aon_timer_get_time_calendar(out_time);
            // shift the hour on the RV chip:
            rv_set_time(out_time);
        }

        dst_alarm_pending = false;
        _set_dst_alarm();      
    }

    // once per day sync the RP2350 RTC with the more accurate RV-3028-C7
    if (out_time->tm_mday != mday) {
        mday = out_time->tm_mday;

        rv_get_time(out_time);
        rtc_set_time(out_time, false, true);
    }

    return true;
}

bool rtc_is_time_valid(void) {
    return time_valid;
}

void rtc_get_epoch(struct timespec *ts) {
    aon_timer_get_time(ts);
}

void rtc_prep_sleep(bool going_to_sleep) {
    if (going_to_sleep) {
        rp2350_in_deep_sleep = true;
        powman_timer_set_1khz_tick_source_lposc();
    } else {
        rp2350_in_deep_sleep = false;
        powman_timer_set_1khz_tick_source_xosc();
    }    
}

static void alarm_fired(void) {
    printf("DST alarm fired!!\n");
    aon_timer_disable_alarm();

    dst_alarm_pending = true;

    if (rp2350_in_deep_sleep) {
        // TODO: enter_deep_sleep();
    }
}



static void _set_dst_alarm(void) {
    printf("setting DST alarm\n");
    struct tm now;
    aon_timer_get_time_calendar(&now);

    uint16_t year = now.tm_year + 1900;

    bool before_mar_jump = _before_mar_transition(&now);
    bool before_nov_jump = _before_nov_transition(&now);

    if (before_mar_jump) {
        now.tm_mon = 2;
        now.tm_mday = _nth_sunday_dom(year, 3, 2);
        now.tm_hour = 2;
        now.tm_min = 0;
        now.tm_sec = 0;
    } else if (before_nov_jump) {
        now.tm_mon = 10;
        now.tm_mday = _nth_sunday_dom(year, 11, 1);
        now.tm_hour = 2;
        now.tm_min = 0;
        now.tm_sec = 0;
    } else {
        now.tm_mon = 2;
        now.tm_mday = _nth_sunday_dom(year + 1, 3, 2);
        now.tm_hour = 2;
        now.tm_min = 0;
        now.tm_sec = 0;
        now.tm_year += 1;
    }
    printf("DST alarm: %04d-%02d-%02d %02d:%02d:%02d\n",
           now.tm_year + 1900,
           now.tm_mon + 1,
           now.tm_mday,
           now.tm_hour,
           now.tm_min,
           now.tm_sec);
    aon_timer_enable_alarm_calendar(&now, alarm_fired, true);
}

static bool _before_mar_transition(const struct tm *now) {
    if (now->tm_mon < 2) return true;
    if (now->tm_mon > 2) return false;
    if (now->tm_mday < _nth_sunday_dom(now->tm_year+1900, 3, 2)) return true;
    if (now->tm_mday > _nth_sunday_dom(now->tm_year+1900, 3, 2)) return false;

    return (now->tm_hour < 2);
}

static bool _before_nov_transition(const struct tm *now) {
    if (now->tm_mon < 10) return true;
    if (now->tm_mon > 10) return false;
    if (now->tm_mday < _nth_sunday_dom(now->tm_year+1900, 11, 1)) return true;
    if (now->tm_mday > _nth_sunday_dom(now->tm_year+1900, 11, 1)) return false;

    return (now->tm_hour < 2); 
}


static bool _should_be_dst(const struct tm *time) {
    if (!time) return false;

    uint16_t year = time->tm_year + 1900;
    uint8_t month = time->tm_mon + 1;
    uint8_t day = time->tm_mday;
    uint8_t hour = time->tm_hour;

    if (month < 3 || month > 11) return false;
    if (month > 3 && month < 11) return true;

    uint8_t dst_start_dom = _nth_sunday_dom(year, 3, 2);
    uint8_t dst_end_dom = _nth_sunday_dom(year, 11, 1);
    
    if (month == 3) {
        if (day < dst_start_dom) return false;
        if (day > dst_start_dom) return true;
        return (hour >= 2);
    } else { //month == 11
        if (day < dst_end_dom) return true;
        if (day > dst_end_dom) return false;
        return (hour < 2);
    }
}

// static bool _is_leap_year(uint16_t year) {
//     return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
// }

static uint8_t _dow(uint16_t year, uint8_t month, uint8_t day) {
    year -= (month < 3);
    return (year + year/4 - year/100 + year/400 + sakamoto_algo[month - 1] + day) % 7;
}

static uint8_t _nth_sunday_dom(uint16_t year, uint8_t month, uint8_t n) {
    uint8_t dow1 = _dow(year, month, 1);
    uint8_t first_sunday = (dow1 == 0) ? 1 : (8 - dow1);
    return first_sunday + (n - 1) * 7;
}

bool rtc_get_time_string(char *time_string) {
    struct tm now;
    rtc_get_time(&now);

    time_string[4] = '-';
    time_string[7] = '-';
    time_string[10] = ' ';
    time_string[13] = ':';

    uint16_t year = now.tm_year + 1900;
    time_string[0] = (year / 1000) + 0x30;
    time_string[1] = (year % 1000 / 100) + 0x30;
    time_string[2] = (year % 100 / 10) + 0x30;
    time_string[3] = (year % 10) + 0x30;

    uint8_t month = now.tm_mon + 1;
    time_string[5] = (month / 10) + 0x30;
    time_string[6] = (month % 10) + 0x30;

    uint8_t mday = now.tm_mday;
    time_string[8] = (mday / 10) + 0x30;
    time_string[9] = (mday % 10) + 0x30;

    uint8_t hour = now.tm_hour;
    time_string[11] = (hour / 10) + 0x30;
    time_string[12] = (hour % 10) + 0x30;

    uint8_t min = now.tm_min;
    time_string[14] = (min / 10) + 0x30;
    time_string[15] = (min % 10) + 0x30;
}