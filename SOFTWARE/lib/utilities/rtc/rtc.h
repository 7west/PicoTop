#pragma once

/**     rtc.h
 * This library will keep local wall time on the RP2350's Always-On Timer and let
 * it function as an RTC. 
 */

#include <time.h>
#include <stdbool.h>


// starts AON Timer, sets dummy time
bool rtc_init(void);

// sets the time
//  set_rv - whether to set RV-3028-C7 time as well as AON time
//  change_dst - whether to mess with/calculate DST with this setting of the time
bool rtc_set_time(const struct tm *in_time, bool set_rv, bool change_dst);

// returns time, but checks for DST event before returning
//      will +/- hour if time must be corrected
bool rtc_get_time(struct tm *out_time);

// WRITES 16 CHAR TO STRING. calls rtc_get_time()
bool rtc_get_time_string(char *time_string);

// called by whatever library puts us in/out of deep sleep
void rtc_prep_sleep(bool going_to_sleep);

// returns true if time has been set. False if time set to dummy reset time
bool rtc_is_time_valid(void);

void rtc_get_epoch(struct timespec *ts);

// Helper functions

/**
 * @brief Called by AON Timer Alarm.
 * 
 * Disables alarm. Sets flag. Will go back to sleep if in deep sleep.
 * 
 */
static void alarm_fired(void);

// sets an AON Timer alarm for the next DST event
static void _set_dst_alarm(void);

// test if now is before March DST transition
static bool _before_mar_transition(const struct tm *now);

// test if now is before November DST transition
static bool _before_nov_transition(const struct tm *now);

// check if time is DST or not
static bool _should_be_dst(const struct tm *time);

// static bool _is_leap_year(uint16_t year);

static uint8_t _dow(uint16_t year, uint8_t month, uint8_t day);

static uint8_t _nth_sunday_dom(uint16_t year, uint8_t month, uint8_t n);