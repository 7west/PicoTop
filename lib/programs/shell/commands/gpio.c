#include "../commands.h"
#include "hardware/gpio.h"





bool cmd_gpio_help(const out_t *out) {
    out->write(out->ctx, "gpio -p [8/9] [-i] [-o/-r] [1/0]\nSelect pin, input/output/resistor, and high/low if output or pullup resistor\nOnly one option (r,i,o) allowed per run");
    return true;
}

bool cmd_gpio_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_gpio_help(out);
}

// gpio -p 8 -i
// gpio -p 8 -r 1
// gpio -p 9 -o 1

bool cmd_gpio(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_gpio_help(out);
    }

    if (argc < 3 || argc > 5) {
        return cmd_gpio_fail(out);
    }

    bool p_flag = false;
    bool i_flag = false;
    bool o_flag = false;
    bool r_flag = false;
    uint8_t pin = 0;
    uint8_t val = 0;

    getopt_state_t state;
    getopt_init(&state);

    int32_t c;
    while ((c = getopt(argc, argv, "p:io:r:", &state)) != -1) {

        switch (c) {
            case 'p':
                p_flag = true;
                pin = *state.optarg - '0';
                break;
            case 'i':
                i_flag = true;
                break;
            case 'o':
                o_flag = true;
                val = *state.optarg - '0';
                break;
            case 'r':
                r_flag = true;
                val = *state.optarg - '0';
                break;
            default:
                return cmd_gpio_fail(out);
        }
    }

    if (!p_flag) return cmd_gpio_fail(out);
    if ((o_flag + i_flag + r_flag) != 1) return cmd_gpio_fail(out);


    if (i_flag) {
        gpio_set_dir(pin, GPIO_IN);

        if (gpio_get(pin)) {
            out->write(out->ctx, "Pin reports HIGH");
        } else {
            out->write(out->ctx, "Pin reports LOW");
        }
        return true;
    }

    if (o_flag) {
        if (val != 1 && val != 0) return cmd_gpio_fail(out);

        gpio_set_dir(pin, GPIO_OUT);

        gpio_put(pin, val);
        if (val) {
            out->write(out->ctx, "outputting HIGH");
        } else {
            out->write(out->ctx, "outputting LOW");
        }

        return true;
    }

    if (r_flag) {
        if (val != 1 && val != 0) return cmd_gpio_fail(out);
        
        if (val) {
            gpio_pull_up(pin);
            out->write(out->ctx, "enabling pull-up resistor");
        } else {
            gpio_pull_down(pin);
            out->write(out->ctx, "enabling pull-down resistor");
        }

        return true;
    }
}