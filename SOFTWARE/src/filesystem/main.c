#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "terminal.h"
#include "os_debug.h"
#include "sd_block.h"
#include "vfs.h"
#include "fat16.h"


static void core1_main(void) {

    term_init();

    // so DMA always overrules USB, otherwise DVI signal can choke and die
    irq_set_priority(DMA_IRQ_0, 0);
    irq_set_priority(USBCTRL_IRQ, 0xC0);

    while (1)
        __wfi();
}



uint8_t buffer[1472];


int main(void) {
    stdio_init_all();

    printf("START\n");

    multicore_launch_core1(core1_main);

    gpio_init(5);
    gpio_set_dir(5, GPIO_IN);
    gpio_pull_up(5);

    // sleep_ms(3000);

    term_clear_screen();

    term_set_cursor(0,29);
    term_puts("7West");
    term_set_cursor(0,0);


    if (vfs_init()) {
        printf("VFS Init Successful\n");
    } else {
        printf("VFS Init FAILED\n");
        debug_print_log_UART();
    }

    uint64_t time_pressed = time_us_64();


    while(true) {

        if (!gpio_get(5) && time_pressed + 500000 < time_us_64()) {
            time_pressed = time_us_64();

            if (vfs_exists("getty.txt")) {
                printf("file getty.txt exists\n");
            }
            if (vfs_exists("docs/novel")) {
                printf("dir /docs/novel/ exists");
            }
            if (vfs_exists("/logs/")) {
                printf("logs exists???????\n");
            }


        }

    }
}