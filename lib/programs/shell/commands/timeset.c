
#include "../commands.h"
#include "rtc.h"




bool cmd_timeset_help(const out_t *out) {
    out->write(out->ctx, "set time with format: YYYY-MM-DD HH:MM:SS\nTop UI Bar time may be off for 1 minute after setting. System time is correct");
    return true;
}

bool cmd_timeset_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_timeset_help(out);
}

bool cmd_timeset(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_timeset_help(out);
    }

    if (argc != 3) {
        return cmd_timeset_fail(out);
    }
    if ((strlen(argv[1]) != 10) || (strlen(argv[2]) != 8)) {
        return cmd_timeset_fail(out);
    }

    // parse argv[1] and argv[2] and convert to ints to struct tm
    //  only validate month, hour, & minute. Why validate it all, this should catch 99% of errors
    struct tm now = {0};
    uint16_t year = 0;
    year += (argv[1][0] - '0') * 1000;
    year += (argv[1][1] - '0') * 100;
    year += (argv[1][2] - '0') * 10;
    year += (argv[1][3] - '0');
    now.tm_year = year - 1900;

    uint8_t month = 0;
    month += (argv[1][5] - '0') * 10; 
    month += (argv[1][6] - '0');
    if (month > 0 && month < 13) {
        now.tm_mon = month - 1;
    } else {
        return cmd_timeset_fail(out);
    }

    uint8_t mday = 0;
    mday += (argv[1][8] - '0') * 10;  
    mday += (argv[1][9] - '0');
    if (mday > 0 && mday < 32) {
        now.tm_mday = mday;
    } else {
        return cmd_timeset_fail(out);
    }

    uint8_t hour = 0;
    hour += (argv[2][0] - '0') * 10;
    hour += (argv[2][1] - '0');
    if (hour >= 0 && hour < 24) {
        now.tm_hour = hour;
    } else {
        return cmd_timeset_fail(out);
    }

    now.tm_min += (argv[2][3] - '0') * 10;
    now.tm_min += (argv[2][4] - '0');
    now.tm_sec += (argv[2][6] - '0') * 10;
    now.tm_sec += (argv[2][7] - '0');
    
    if (rtc_set_time(&now, true, true)) {
        out->write(out->ctx, "RTC time set");
    } else {
        out->write(out->ctx, "Setting time failed");
    }

    update_ui_time();

    return true;
}