#include "includes.h"


#define GPIO_5V_EN 29
#define GPIO_SLEEP_SWITCH 11


// defined in main-fns.c
void init_os(void);
void check_periodic_min(void);
void draw_ui_bar(void);
void update_ui_time(void);

static void core1_main(void) {

    gpio_init(GPIO_5V_EN);
    gpio_set_dir(GPIO_5V_EN, GPIO_OUT);
    gpio_put(GPIO_5V_EN, 1);
    
    // sleep_ms(2000);

    buzz_init_pwm();

    term_init();

    keyboard_init();
    irq_set_enabled(USBCTRL_IRQ, true);

    // so DMA always overrules USB, otherwise DVI signal can choke and die
    irq_set_priority(DMA_IRQ_0, 0);
    irq_set_priority(USBCTRL_IRQ, 0xC0);

    debug_deposit("CORE-INFO-Core1 started up successfully.", 0, DBG_NULL_VAR);

    // gpio_init(GPIO_SLEEP_SWITCH);
    // gpio_set_dir(GPIO_SLEEP_SWITCH, GPIO_IN);
    // gpio_disable_pulls(GPIO_SLEEP_SWITCH);

    bool last_switch_state = true;

    while (1) {

        // if (gpio_get(GPIO_SLEEP_SWITCH) && !last_switch_state) {
        //     printf("5V on, disp on\n");
        //     gpio_put(GPIO_5V_EN, 1);

        //     sleep_ms(3000);
        //     video_out_resume();
        //     last_switch_state = true;
        //     printf("done\n");

        // } else if (!gpio_get(GPIO_SLEEP_SWITCH) && last_switch_state) {
        //     printf("5V off, disp off\n");
        //     gpio_put(GPIO_5V_EN, 0);

        //     video_out_pause();
        //     last_switch_state = false;
        //     sleep_ms(1000);
        //     printf("done\n");

        // }

        keyboard_loop_call();
        buzz_loop_call();
        
        // if (gpio_get(GPIO_SLEEP_SWITCH)) {
            
        // }
        

        __wfi(); // CRITICAL
    }
        
}

int main(void) {
    stdio_init_all();

    printf("START\n");
    // critical delay for monitor to reset and accept new DVI signal
    //  may need to be 3s. We can't control this. Monitor is other hardware
    sleep_ms(2000);

    // core1: keyboard and display handling
    multicore_launch_core1(core1_main);

    // sleep_ms(2000);

    // turn off USB IRQ on core0, so core1 handles it
    irq_set_enabled(USBCTRL_IRQ, false);

    init_os();
    // term_clear_screen();
    // term_set_cursor(0,0);

    // for button to print debug log
    uint64_t time_pressed = time_us_64();

    // key_event_t key;

    while(true) {

        key_event_t key;
        if (keyboard_queue_pop(&key)) {

            if (key.keycode == KBD_BASCII_F12) {
                debug_print_log_UART();
                continue;
            }

            prog_get_active()->on_key(key);
        }
        




        






        check_periodic_min();
    }
}

