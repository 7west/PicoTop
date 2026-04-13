#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "token.h"
#include "vars.h"





// Static Functions:

static bool parse_num(const char *str, size_t *consumed, double *value);
static bool parse_num_hex(const char *str, size_t *consumed, double *value);
static bool parse_num_bin(const char *str, size_t *consumed, double *value);
static bool parse_sci_notation(const char *str, size_t *consumed, double *value);
static bool is_numerical(const char c);
static bool is_hex_digit(const char c);
static bool is_bin_digit(const char c);
static uint8_t hex_to_dec(const char c);

static bool parse_name(const char *str, size_t *consumed, char *name_buf);
static bool is_alphabetic(const char c);
static void lowercase_buf(char *buf, uint8_t len);

static bool parse_op_symbol(const char c, OpType *op, TokenType prev_token_type);
static bool is_op_single_symbol(const char c);
static bool is_op_double_symbol(const char c);





bool tokenizer_init(Tokenizer *t, const char *input) {
    if (t == NULL) return false;
    if (input == NULL) return false;

    t->input = input;
    t->cur_pos = 0;
    t->prev_token_type = TOKEN_NULL;
    memset(t->name_buf, 0x00, VAR_NAME_MAX+1);

    return true;
}


// the big chalupa
calc_return_t tokenizer_next(Tokenizer *t, Token *out) {

    while (t->input[t->cur_pos] == ' ') t->cur_pos += 1;

    printf("eating char '%c'\n", t->input[t->cur_pos]);

    // === 0-9 or '.'?
    if (is_numerical(t->input[t->cur_pos]) || (t->input[t->cur_pos] == '.')) {
        if (!parse_num(t->input, &t->cur_pos, &out->data.number)) {
            return CALC_RETURN_NUMBER_PARSE_FAIL;
        }
        printf("got number: %f\n", out->data.number);
        out->type = TOKEN_NUM;


    // === a-z, A-Z ?
    } else if (is_alphabetic(t->input[t->cur_pos])) {
        if (!parse_name(t->input, &t->cur_pos, t->name_buf)) {
            return CALC_RETURN_NAME_PARSE_FAIL;
        }
        
        if (funcs_get_idx(t->name_buf, &out->data.func_index)) {
            out->type = TOKEN_FUNC;
        } else {
            memcpy(out->data.var_name, t->name_buf, VAR_NAME_MAX+1);
            out->type = TOKEN_VAR;
        }



    // === paren?
    } else if (t->input[t->cur_pos] == '('){
        out->type = TOKEN_LPAREN;
        t->cur_pos += 1;

    } else if (t->input[t->cur_pos] == ')') {
        out->type = TOKEN_RPAREN;
        t->cur_pos += 1;


    // === comma?
    } else if (t->input[t->cur_pos] == ',') {
        out->type = TOKEN_COMMA;
        t->cur_pos += 1;


    // double symbol (and *) <<,>>,**
    //  This block handles '*' (mult) as well because of the way the if-else works
    } else if (is_op_double_symbol(t->input[t->cur_pos])) {

        if (t->input[t->cur_pos] == '*') {
            if (t->input[t->cur_pos + 1] == '*') {
                out->type = TOKEN_OP;
                out->data.op = OP_POW;
                t->cur_pos += 2;
            } else {
                out->type = TOKEN_OP;
                out->data.op = OP_MUL;
                t->cur_pos += 1;
            }
        } else if (t->input[t->cur_pos] == '<') {
            if (t->input[t->cur_pos + 1] == '<') {
                out->type = TOKEN_OP;
                out->data.op = OP_SHIFT_L;
                t->cur_pos += 2;
            } else {
                return CALC_RETURN_OP_PARSE_FAIL;
            }
        } else if (t->input[t->cur_pos] == '>') {
            if (t->input[t->cur_pos + 1] == '>') {
                out->type = TOKEN_OP;
                out->data.op = OP_SHIFT_R;
                t->cur_pos += 2;
            } else {
                return CALC_RETURN_OP_PARSE_FAIL;
            }
        }
        printf("op type: %d\n", out->data.op);

    // === symbol? (except multiplication)
    } else if (is_op_single_symbol(t->input[t->cur_pos])) {
        if (!parse_op_symbol(t->input[t->cur_pos], &out->data.op, t->prev_token_type)) {
            return CALC_RETURN_OP_PARSE_FAIL;
        }
        out->type = TOKEN_OP;
        t->cur_pos += 1;
        printf("op type: %d\n", out->data.op);

    // === \0?
    } else if (t->input[t->cur_pos] == '\0') {

        out->type = TOKEN_END;

    } else { // unallowed char
        return CALC_RETURN_TOKEN_FAIL;
    }

    memset(t->name_buf, 0x00, VAR_NAME_MAX+1);
    t->prev_token_type = out->type;
    return CALC_RETURN_SUCCESS;
}

// str + consumed always equals the char after the number
static bool parse_num(const char *str, size_t *consumed, double *value) {

    if (str == NULL) return false;
    if (consumed == NULL) return false;
    if (value == NULL) return false;

    *value = 0;
    bool decimal = false;

    // === starts with 0?
    if (str[*consumed] == '0') {

        // hex or bin?
        if (str[*consumed + 1] == 'x') {
            *consumed += 2;
            return parse_num_hex(str, consumed, value);
        }
        if (str[*consumed + 1] == 'b') {
            *consumed += 2;
            return parse_num_bin(str, consumed, value);
        }
        // decimal?
        if (str[*consumed + 1] == '.') {
            *consumed += 2;
            decimal = true;
        }
        // just weird leading zero?
    }
    
    double div = 1.0;
    double val_decimal = 0;
    // === loop w/decimal check
    while (str[*consumed] != '\0') {

        // check for decimal
        if (str[*consumed] == '.') {

            if (!decimal) {
                decimal = true; 
            } else {
                *consumed += 1;
                return false; // 2 decimal points. Bad.
            }
            *consumed += 1;
            continue;
        } else if (str[*consumed] == 'e' || str[*consumed] == 'E') { 
            if (decimal) {
                *value += val_decimal;
            }
            *consumed += 1;
            return parse_sci_notation(str, consumed, value);

        } else if (!is_numerical(str[*consumed])) {
            if (decimal) {
                *value += val_decimal;
            }
            return true; // done parsing number
        }

        // loop add
        if (decimal) {
            div *= 10.0;
            val_decimal += (str[*consumed] - '0') / div;
        } else {
            *value = (*value * 10) + (str[*consumed] - '0');
        }

        *consumed += 1;
    }
    if (decimal) {
        *value += val_decimal;
    }

    return true;
}

static bool parse_sci_notation(const char *str, size_t *consumed, double *value) {
    if (str == NULL) return false;
    if (consumed == NULL) return false;
    if (value == NULL) return false;

    bool neg_exp = false;

    if (str[*consumed] == '+') {
        *consumed += 1;
    } else if (str[*consumed] == '-') {
        neg_exp = true;
        *consumed += 1;
    }

    if (!is_numerical(str[*consumed])) return false;

    int32_t exponent = 0;
    while (str[*consumed] != '\0' && is_numerical(str[*consumed])) {
        exponent = exponent * 10 + (str[*consumed] - '0');
        *consumed += 1;
    }

    double mult = 1.0;
    for (uint32_t i = 0; i < exponent; i++) {
        mult *= 10.0;
    }

    if (neg_exp) {
        *value /= mult;
    } else {
        *value *= mult;
    }

    return true;
}

// takes a NON-ZERO consumed from parse_num(). Do not zero it.
// value is already zero
static bool parse_num_hex(const char *str, size_t *consumed, double *value) {
    if (str == NULL) return false;
    if (consumed == NULL) return false;
    if (value == NULL) return false;

    if (!is_hex_digit(str[*consumed])) return false;

    while (str[*consumed] != '\0') {
        
        if (is_hex_digit(str[*consumed])) {
            *value = *value * 16 + hex_to_dec(str[*consumed]);
        } else {
            return true; // done with hex characters
        }
        *consumed += 1;
    }
    return true; // done with string
}

// takes a NON-ZERO consumed from parse_num(). Do not zero it.
// value is already zero
static bool parse_num_bin(const char *str, size_t *consumed, double *value) {
    if (str == NULL) return false;
    if (consumed == NULL) return false;
    if (value == NULL) return false;

    if (!is_bin_digit(str[*consumed])) return false;

    while (str[*consumed] != '\0') {

        if (is_bin_digit(str[*consumed])) {
            *value = *value * 2 + (str[*consumed] - '0');
        } else {
            return true; // done with bin chars
        }
        *consumed += 1;
    }
    return true; // done with string
}

static bool is_numerical(const char c) {
    if (c >= '0' && c <= '9') return true;
    return false;
}

static bool is_hex_digit(const char c) {
    char safe_c = c;
    // capitalize c if it is a letter
    if (c >= 'a' && c <= 'f') safe_c -= 32;

    if (is_numerical(safe_c) || (safe_c >= 'A' && safe_c <= 'F')) return true;
    return false;
}

static bool is_bin_digit(const char c) {
    if (c == '0' || c == '1') return true;
    return false;
}



// assumes c is 0-9, a-f, A-F
static uint8_t hex_to_dec(const char c) {
    char safe_c = c;
    // capitalize c if it is a letter
    if (c >= 'a' && c <= 'f') safe_c -= 32;

    if (is_numerical(safe_c)) {
        return safe_c - '0';
    } else {
        return (safe_c - 'A') + 10;
    }
}




static bool parse_name(const char *str, size_t *consumed, char *name_buf) {
    
    if (str == NULL) return false;
    if (consumed == NULL) return false;
    if (name_buf == NULL) return false;

    size_t name_len = 0;

    while (str[*consumed] != '\0') {
        if (is_alphabetic(str[*consumed]) || is_numerical(str[*consumed])) {
            *consumed += 1;
            name_len += 1;
        } else {
            break;
        }
    }
    if (name_len > VAR_NAME_MAX) return false;
    if (name_len == 0) return false;

    memcpy(name_buf, str + (*consumed - name_len), name_len);
    lowercase_buf(name_buf, (uint8_t) name_len);
    
    name_buf[name_len] = '\0';
    return true;
}

static bool is_alphabetic(const char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
    return false;
}


static void lowercase_buf(char *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++ ) {

        if (buf[i] >= 'A' && buf[i] <= 'Z') {
            buf[i] += 32;
        }
    }
}



// the way the parsing is structured, '*' will never be reached in here
static bool parse_op_symbol(const char c, OpType *op, TokenType prev_token_type) {
    switch (c) {
        case '+':
            *op = OP_ADD;
            break;
        case '-':
            if (prev_token_type == TOKEN_NULL || prev_token_type == TOKEN_OP 
                || prev_token_type == TOKEN_LPAREN || prev_token_type == TOKEN_COMMA) {
                *op = OP_NEGATE;
            } else {
                *op = OP_SUB;
            }
            break;
        case '*': // see comment above
            *op = OP_MUL;
            break;
        case '/':
            *op = OP_DIV;
            break;
        case '%':
            *op = OP_MOD;
            break;
        case '&':
            *op = OP_BIT_AND;
            break;
        case '|':
            *op = OP_BIT_OR;
            break;
        case '^':
            *op = OP_BIT_XOR;
            break;
        case '~':
            *op = OP_BIT_NOT;
            break;
        default:
            return false;
    }
    return true;
}

static bool is_op_single_symbol(const char c) {
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || 
        c == '&' || c == '|' || c == '^' || c == '~' ) {
        
        return true;
    }
    return false;
}

static bool is_op_double_symbol(const char c) {
    if (c == '*' || c == '<' || c == '>') return true;
    return false;
}