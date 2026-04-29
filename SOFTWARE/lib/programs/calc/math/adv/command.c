#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "command.h"
#include "../token.h"
#include "../vars.h"

static char *find_top_level_comma(const char *str);
static bool parse_num_or_var(const char *p, size_t *consumed, double complex *out);


bool command_parse(char *input, parsed_cmd_t *cmd) {
    if (input == NULL || cmd == NULL) return false;

    cmd->argc = 0;

    // Helper: skip leading spaces at a given pointer
    #define SKIP_SPACES(p) while (*(p) == ' ') (p)++

    // --- Detect prefix ---
    struct { const char *prefix; cmd_type_t type; } prefixes[] = {
        { "let ",       CMD_TYPE_LET       },
        // { "deriv ",     CMD_TYPE_DERIV     }, // later feature, not now
        { "nderiv ",    CMD_TYPE_NDERIV    },
        { "solve ",     CMD_TYPE_SOLVE     },
        { "nint ", CMD_TYPE_NINTEGRATE},
    };

    cmd->type = CMD_TYPE_EXPR;
    const char *after_prefix = input;

    for (int i = 0; i < 5; i++) {
        size_t plen = strlen(prefixes[i].prefix);
        if (strncmp(input, prefixes[i].prefix, plen) == 0) {
            cmd->type = prefixes[i].type;
            after_prefix = input + plen;
            break;
        }
    }

    // --- CMD_TYPE_EXPR: whole input is the expression ---
    if (cmd->type == CMD_TYPE_EXPR) {
        cmd->expr_start = input;
        return true;
    }

    // --- CMD_TYPE_LET: "let <var> = <expr>" ---
    if (cmd->type == CMD_TYPE_LET) {
        size_t consumed = 0;
        if (!parse_name(after_prefix, &consumed, cmd->var_name)) return false;

        const char *p = after_prefix + consumed;
        SKIP_SPACES(p);
        if (*p != '=') return false;
        p++;
        SKIP_SPACES(p);
        cmd->expr_start = p;
        printf("%s\n", p);
        return true;
    }

    // --- All remaining types: expr starts at after_prefix, args follow commas ---
    //     Find the comma that ends the expression (top-level only)
    char *expr_end_comma = find_top_level_comma(after_prefix);
    if (expr_end_comma == NULL) return false;

    // replace first comma with \0 for expression parsing in calc_eval() -> tokenizer_next()
    *expr_end_comma = '\0';

    cmd->expr_start = after_prefix;

    // Parse variable name after first comma
    const char *p = expr_end_comma + 1;
    SKIP_SPACES(p);
    size_t consumed = 0;
    if (!parse_name(p, &consumed, cmd->var_name)) return false;
    p += consumed;

    if (cmd->type == CMD_TYPE_DERIV) {
        // No numeric args needed
        return true;
    }

    // All remaining types need at least one numeric arg after var
    const char *next_comma = find_top_level_comma(p);
    if (next_comma == NULL) {
        // "solve" allows omitting initial guess — default to 1.0
        if (cmd->type == CMD_TYPE_SOLVE) {
            cmd->args[0] = 1.0;
            cmd->argc = 1;
            return true;
        }
        return false;
    }

    p = next_comma + 1;
    SKIP_SPACES(p);
    bool negate = (*p == '-');
    if (negate) p++;
    consumed = 0;
    // if (!parse_num(p, &consumed, &cmd->args[0])) return false;
    if (!parse_num_or_var(p, &consumed, &cmd->args[0])) return false;
    cmd->argc = 1;
    p += consumed;
    if (negate) cmd->args[0] = -cmd->args[0];

    if (cmd->type == CMD_TYPE_NDERIV || cmd->type == CMD_TYPE_SOLVE) {
        return true;
    }

    // CMD_TYPE_NINTEGRATE needs upper bound too
    next_comma = find_top_level_comma(p);
    if (next_comma == NULL) return false;

    p = next_comma + 1;
    SKIP_SPACES(p);
    consumed = 0;
    negate = (*p == '-');
    if (negate) p++;
    // if (!parse_num(p, &consumed, &cmd->args[1])) return false;
    if (!parse_num_or_var(p, &consumed, &cmd->args[1])) return false;
    cmd->argc = 2;
    if (negate) cmd->args[1] = -cmd->args[1];

    return true;

    #undef SKIP_SPACES
}


static char *find_top_level_comma(const char *str) {
    uint8_t depth = 0;
    const char *current = str;

    while (*current != '\0') {
        if (*current == '(') {
            depth += 1;
        } else if (*current == ')') {
            depth -= 1;
        } else if (*current == ',' && depth == 0) {
            return (char *)current;
        }
        current += 1;
    }
    return NULL;
}


static bool parse_num_or_var(const char *p, size_t *consumed, double complex *out) {

    double real_val = 0.0;
    size_t c = 0;
    if (parse_num(p, &c, &real_val) && c > 0) {
        *consumed += c;
        *out = CMPLX(real_val, 0.0);
        return true;
    }
    c = 0;
    char name_buf[VAR_NAME_MAX + 1] = {0};
    if (!parse_name(p, &c, name_buf)) return false;
    *consumed += c;
    
    return vars_get(name_buf, out);
}