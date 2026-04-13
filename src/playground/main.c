#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "terminal.h"
#include "os_debug.h"
#include "usb_kbd.h"

#include "bsp/board.h"
#include "tusb.h"
#include "tusb_config.h"

#define GPIO_LOG_OUTPUT 5
#define GPIO_5V_EN 29

static void core1_main(void) {

    term_init();

    keyboard_init();
    irq_set_enabled(USBCTRL_IRQ, true);
    
    // so DMA always overrules USB, otherwise DVI signal can choke and die
    irq_set_priority(DMA_IRQ_0, 0);
    irq_set_priority(USBCTRL_IRQ, 0xC0);

    while (1) {
        keyboard_loop_call();
        __wfi(); // remove?
    }
        
}

int main(void) {
    stdio_init_all();

    printf("START\n");

    gpio_init(GPIO_5V_EN);
    gpio_set_dir(GPIO_5V_EN, GPIO_OUT);
    gpio_put(GPIO_5V_EN, 1);

    sleep_ms(2000);

    multicore_launch_core1(core1_main);
    irq_set_enabled(USBCTRL_IRQ, false);

    gpio_init(GPIO_LOG_OUTPUT);;
    gpio_set_dir(GPIO_LOG_OUTPUT, GPIO_IN);
    gpio_pull_up(GPIO_LOG_OUTPUT);

    sleep_ms(3000);

    term_clear_screen();

    term_set_cursor(0,29);
    term_puts("7West");
    term_set_cursor(0,0);

    // for button to print debug log
    uint64_t time_pressed = time_us_64();

    while(true) {

        // int c = getchar_timeout_us(0);

        // if (c >= 0) {
        //     if (c == 0x0A) {
        //         term_new_line();
        //     } else if (c == 0x7F) {
        //         term_backspace();
        //     } else {
        //         printf("%c", (char)c);
        //         term_putc(c);
        //     }
        // }


        if (!gpio_get(GPIO_LOG_OUTPUT) && time_pressed + 500000 < time_us_64()) {
            time_pressed = time_us_64();
            debug_print_log_UART();
        }

        key_event_t key;
        if (keyboard_queue_pop(&key)) {

            if (key.keycode >= 0) {
                if (key.keycode == '\n') {
                    term_new_line();
                } else if (key.keycode == '\b') {
                    term_backspace();
                } else {
                    printf("%c", (char)key.keycode);
                    term_putc(key.keycode);
                }
            }
        }



    }
}
