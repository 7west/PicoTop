#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "terminal.h"
#include "os_debug.h"
#include "usb_kbd.h"
#include "video_out.h"
// #include "rtc.h"
// #include "prog.h"
// #include "shell.h"
#include "fat16.h"
// #include "sd_block.h"

#include "bsp/board.h"
#include "tusb.h"
#include "tusb_config.h"

#define GPIO_5V_EN 29
#define GPIO_CHG_DONE 2
#define GPIO_LBO_CHG 3
#define GPIO_BUZZ_PWM 10
#define GPIO_SLEEP_SWITCH 11
#define GPIO_BATT_ADC 28

/**TODO:
 * GPIO29 - 5V_EN
 * GPIO28/ADC2 - Battery Voltage
 * GPIO2 - /CHG_DONE
 * GPIO3 - /LBO_CHG
 * GPIO8 - button?
 * GPIO10 - Buzzer (PWM5A)
 * GPIO11 - Sleep Switch
 * 
 * DVI:
 * GPIO12
 * GPIO13
 * GPIO14
 * GPIO15
 * GPIO16
 * GPIO17
 * GPIO18
 * GPIO19
 * 
 * SD:
 * GPIO20 - SPI CIPO
 * GPIO21 - SPI CS
 * GPIO22 - SPI SCK
 * GPIO23 - SPI COPI
 * GPIO24 - SPI SD CD
 */


static void core1_main(void) {

    // gpio_init(GPIO_5V_EN);
    // gpio_set_dir(GPIO_5V_EN, GPIO_OUT);
    // gpio_put(GPIO_5V_EN, 1);

    term_init();

    keyboard_init();
    irq_set_enabled(USBCTRL_IRQ, true);
    
    // so DMA always overrules USB, otherwise DVI signal can choke and die
    irq_set_priority(DMA_IRQ_0, 0);
    irq_set_priority(USBCTRL_IRQ, 0xC0);

    gpio_init(GPIO_SLEEP_SWITCH);
    gpio_set_dir(GPIO_SLEEP_SWITCH, GPIO_IN);
    // gpio_pull_up(GPIO_SLEEP_SWITCH);
    gpio_disable_pulls(GPIO_SLEEP_SWITCH);

    gpio_init(GPIO_5V_EN);
    gpio_set_dir(GPIO_5V_EN, GPIO_OUT);
    gpio_put(GPIO_5V_EN, 1);

    bool last_switch_state = true;

    while (1) {

        // printf("Sleep Switch State: %u\n", gpio_get(GPIO_SLEEP_SWITCH));

        if (gpio_get(GPIO_SLEEP_SWITCH) && !last_switch_state) {
            printf("5V off, disp off\n");

            video_out_resume();
            last_switch_state = true;
            gpio_put(GPIO_5V_EN, 1);

        } else if (!gpio_get(GPIO_SLEEP_SWITCH) && last_switch_state) {
            printf("5V on, disp on\n");
            // video_out_pause();
            video_out_setup();
            last_switch_state = false;
            gpio_put(GPIO_5V_EN, 0);

        }

        keyboard_loop_call();

        if (gpio_get(GPIO_SLEEP_SWITCH)) {
            // __wfi(); 
        }

    }
        
}


int main(void) {
    stdio_init_all();

    printf("START\n");

    sleep_ms(2000);

    adc_init();
    adc_gpio_init(GPIO_BATT_ADC);
    adc_select_input(2);

    gpio_set_function(GPIO_BUZZ_PWM, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(GPIO_BUZZ_PWM);
    const uint16_t top = 999;
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, top);
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_init(slice_num, &cfg, true);

    multicore_launch_core1(core1_main);
    irq_set_enabled(USBCTRL_IRQ, false);

    sleep_ms(2000);
    term_clear_screen();
    term_draw_string(1, 0, "7West");
    term_invert_line(0);

    gpio_init(GPIO_CHG_DONE);
    gpio_set_dir(GPIO_CHG_DONE, GPIO_IN);
    gpio_pull_up(GPIO_CHG_DONE);
    printf("/CHG_DONE: %u\n", gpio_get(GPIO_CHG_DONE));

    gpio_init(GPIO_LBO_CHG);
    gpio_set_dir(GPIO_LBO_CHG, GPIO_IN);
    gpio_pull_up(GPIO_LBO_CHG);
    printf("/LBO_CHG: %u\n", gpio_get(GPIO_LBO_CHG));




    if (fat_mount_sd(true)) {
        printf("FAT Init success\n");
    } else {
        printf("FAT init FAILED\n");
        debug_print_log_UART();
    }
    
    term_set_cursor(0,1);
    
    const float f0 = 1000.0f;
    const float f1 = 2000.0f;
    const int steps = 300;
    const int step_ms = 1;
    while(true) {

        // for (int i = 0; i <= steps; i++) {
        //     float f = f0 + (f1 - f0) * ((float)i / (float)steps);

        //     uint32_t fsys = 150000000;

        //     float div = (float)fsys / (f * (float)(top + 1));
        //     if (div < 1.0f) div = 1.0f;

        //     // pwm_set_clkdiv(slice_num, div);
        //     // pwm_set_gpio_level(GPIO_BUZZ_PWM, (top + 1) / 2);


        //     sleep_ms(step_ms);
        // }

        // sleep_ms(1000);
        



        key_event_t key;
        if (keyboard_queue_pop(&key)) {

            if (key.keycode >= 0) {
                term_putc(key.keycode);
                printf("%c", (char)key.keycode);
                // if (key.keycode == 'f') {
                //     fat_flush();
                // }
            }
        }
        // sleep_ms(2000);

        uint16_t counts = adc_read();
        uint32_t voltage = (165 * counts) >> 10;

        // printf("Raw counts: %u, Voltage*100: %u\n", counts, voltage);


    }
}

