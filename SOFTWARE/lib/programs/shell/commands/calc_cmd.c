#include "../commands.h"
#include "calc.h"




bool cmd_calc_help(const out_t *out) {
    out->write(out->ctx, "Calculator program\n Can evaluate one expression from shell here, must use no spaces.\n  example: calc 60+9");
    return true;
}

bool cmd_calc_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_calc_help(out);
}

bool cmd_calc(uint8_t argc, const char ** argv, const out_t *out) {
    
    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_calc_help(out);
    }

    if (argc > 2) return cmd_calc_fail(out);

    if (argc == 1) {
        prog_switch(&calc_prog);
        return true;
    } 

    // 2 args, calculate string
    calc_result_t calc_result;

    if (calc_str_eval(argv[1], &calc_result)) {
        out->write(out->ctx, calc_result.data.str_out);
    }    
    

    return true;
}