#include "pico/stdlib.h"

#include "buzz.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

#define GPIO_PWM_BUZZ 10 //PWM5A
#define PWM_SLICE_CHAN 0 // 0 = channel A
#define BUZZ_FREQ_DIV 16
#define CLK_FREQ 150000000 // 150MHz

typedef enum {
    BUZZ_STAGE_TONE,
    BUZZ_STAGE_SILENCE
} stage_type_t;

typedef struct {
    stage_type_t type;
    uint16_t freq;
    uint16_t dur; //in ms
} stage_t;

static spin_lock_t *buzz_lock;
static uint32_t buzz_lock_num;

static uint32_t slice;
static bool pwm_hw_initialized = false;
static bool track_active = false;
static const stage_t *queued_stages = NULL;
static uint8_t stage_idx = 0;
static uint8_t stage_end = 0;
static uint64_t stage_end_us = 0;

// track 0 is reserved. No track 0 allowed.
// confirmation "good" beep
static const stage_t track1[] = { {BUZZ_STAGE_TONE, 660, 80}, {BUZZ_STAGE_SILENCE, 0, 30}, {BUZZ_STAGE_TONE, 880, 80} };
// error beep
static const stage_t track2[] = { {BUZZ_STAGE_TONE, 880, 70}, {BUZZ_STAGE_SILENCE, 0, 30}, {BUZZ_STAGE_TONE, 660, 120} };
// short neutral notif beep
static const stage_t track3[] = { {BUZZ_STAGE_TONE, 880, 70} };
// short "vaguely positive" beep
static const stage_t track4[] = { {BUZZ_STAGE_TONE, 988, 40}, {BUZZ_STAGE_SILENCE, 0, 20}, {BUZZ_STAGE_TONE, 1319, 60} };
// boot up beep
static const stage_t track5[] = { {BUZZ_STAGE_TONE, 660, 160}, {BUZZ_STAGE_SILENCE, 0, 60}, {BUZZ_STAGE_TONE, 880, 160} };

void buzz_init_pwm(void) {
    gpio_set_function(GPIO_PWM_BUZZ, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(GPIO_PWM_BUZZ);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&cfg, BUZZ_FREQ_DIV);
    pwm_init(slice, &cfg, false);

    pwm_set_enabled(slice, false); // make sure it is off
    pwm_hw_initialized = true;

    // These might help values surviving weird behavior and tones getting dropped
    //  it's working for now, leave it.
    track_active = false;
    queued_stages = NULL;
    stage_idx = 0;
    stage_end = 0;
    stage_end_us = 0;

    buzz_lock_num = spin_lock_claim_unused(true);
    buzz_lock = spin_lock_instance(buzz_lock_num);
}

// not in buzz.h file
static void _start_stage(const stage_t *stage) {
    uint64_t now = time_us_64();
    if (stage->type == BUZZ_STAGE_TONE) {
        _pwm_on(stage->freq);
    } else {
        _pwm_off();
    }
    stage_end_us = now + (uint64_t)(stage->dur * 1000);
}

bool buzz_loop_call(void) {
    uint32_t s = spin_lock_blocking(buzz_lock);

    if (!pwm_hw_initialized || !track_active) {
        spin_unlock(buzz_lock, s);
        return false;
    }

    if (time_us_64() < stage_end_us) {
        spin_unlock(buzz_lock, s);
        return true;
    }

    if (queued_stages[stage_idx].type == BUZZ_STAGE_TONE) _pwm_off();

    if (++stage_idx >= stage_end) {
        track_active = false;
        queued_stages = NULL;
        spin_unlock(buzz_lock, s);
        return false;
    }

    _start_stage(&queued_stages[stage_idx]);
    spin_unlock(buzz_lock, s);
    return true;
}


// not in buzz.h file
static bool _start_track(const stage_t *track, uint8_t n) {
    uint32_t s = spin_lock_blocking(buzz_lock);

    if (!pwm_hw_initialized || track_active) return false;

    queued_stages = track;
    stage_end = n;
    stage_idx = 0;
    track_active = true;

    _start_stage(&queued_stages[0]);

    spin_unlock(buzz_lock, s);
    return true;
}

bool buzz_play_library(uint8_t track) {
    if (track_active) return false;
    if (track == 0 || track > 5) return false;

    // zero is reserved

    switch (track) {
        case 1: 
            return _start_track(track1, (uint8_t)(sizeof(track1)/sizeof(track1[0])));

        case 2: 
            return _start_track(track2, (uint8_t)(sizeof(track2)/sizeof(track2[0])));

        case 3: 
            return _start_track(track3, (uint8_t)(sizeof(track3)/sizeof(track3[0])));

        case 4: 
            return _start_track(track4, (uint8_t)(sizeof(track4)/sizeof(track4[0])));

        case 5: 
            return _start_track(track5, (uint8_t)(sizeof(track5)/sizeof(track5[0])));

        default:
            return false;
    }

    return true;
}

bool buzz_play_beep(uint16_t freq, uint16_t duration) {
    if (!pwm_hw_initialized) return false;
    if (freq < 200 || freq > 4000) return false;

    _set_buzz_freq(freq);
    pwm_set_enabled(slice, true);

    sleep_ms(duration);
    pwm_set_enabled(slice, false);

    return true;
}

bool buzz_play_sweep(uint16_t freq_start, uint16_t freq_end, uint16_t duration, uint16_t hold) {
    if (!pwm_hw_initialized) return false;
    if (freq_start < 200 || freq_start > 4000) return false;
    if (freq_end < 200 || freq_end > 4000) return false;
    
    uint8_t step_len = 3; //ms
    uint16_t num_steps = duration / step_len;
    if (num_steps == 0) return false;
    
    int32_t freq_fixed = (int32_t)freq_start << 8;
    int32_t step_fixed = (((int32_t)freq_end - freq_start) << 8) / num_steps;

    pwm_set_enabled(slice, true);

    for (uint16_t i = 0; i <= num_steps; i++) {

        _set_buzz_freq((uint16_t)(freq_fixed >> 8));
        sleep_ms(step_len);

        freq_fixed += step_fixed;
    }

    _set_buzz_freq(freq_end);
    if (hold > 0) sleep_ms(hold);

    pwm_set_enabled(slice, false);
    return true;
}


static inline void _pwm_off(void) {
    pwm_set_enabled(slice, false);
}

static inline void _pwm_on(uint16_t freq) {
    _set_buzz_freq(freq);
    pwm_set_enabled(slice, true);
}


static void _set_buzz_freq(uint16_t freq) {

    uint16_t wrap = _wrap_from_freq(freq);

    pwm_set_wrap(slice, wrap);
    pwm_set_chan_level(slice, PWM_SLICE_CHAN, (wrap + 1) / 2);
}

static uint16_t _wrap_from_freq(uint16_t freq) {
    if (freq == 0) return 0;

    return (uint16_t)(CLK_FREQ / (BUZZ_FREQ_DIV * (uint32_t)freq));
}