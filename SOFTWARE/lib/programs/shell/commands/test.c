#include "../commands.h"
#include "calc.h"





bool cmd_test_help(const out_t *out) {
    out->write(out->ctx, "this command is only for running test commands. Some of these are dangerous so don't just fire this off.");
    return true;
}

bool cmd_test_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_test_help(out);
}

bool cmd_test(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_test_help(out);
    }

    if (argc != 2) {
        return cmd_test_fail(out);
    }

    // char res_str[11];
    // char err_msg[23];
    // double result;

    // if (calc_test_eval(argv[1], &result, err_msg)) {
    //     snprintf(res_str, 11, "%.2f", result);
    //     out->write(out->ctx, res_str);

    // } else {
    //     out->write(out->ctx, err_msg);
    // }

    

    

    return true;
}