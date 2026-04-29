
#include "../ast.h"
#include "calc_types.h"




typedef enum {
    CMD_TYPE_EXPR,
    CMD_TYPE_LET,
    CMD_TYPE_DERIV,
    CMD_TYPE_NDERIV,
    CMD_TYPE_SOLVE,
    CMD_TYPE_NINTEGRATE,
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    const char *expr_start;
    char var_name[VAR_NAME_MAX+1];
    double complex args[4];
    uint8_t argc;
} parsed_cmd_t;





bool command_parse(char *input, parsed_cmd_t *cmd);