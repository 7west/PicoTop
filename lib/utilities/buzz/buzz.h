#pragma once

#include <stdio.h>
#include <stdint.h>

void buzz_init_pwm(void);

bool buzz_loop_call(void);

bool buzz_play_library(uint8_t track);

bool buzz_play_beep(uint16_t freq, uint16_t duration);

bool buzz_play_sweep(uint16_t freq_start, uint16_t freq_end, uint16_t duration, uint16_t hold);

static uint16_t _wrap_from_freq(uint16_t freq);

static void _set_buzz_freq(uint16_t freq);

static inline void _pwm_off(void);

static inline void _pwm_on(uint16_t freq);

