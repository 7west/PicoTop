#pragma once

#include "ast.h"

typedef enum { // identifiers
    TOKEN_NULL = 0,
    TOKEN_NUM, // 0-9, .
    TOKEN_VAR, // a-z
    TOKEN_FUNC, // a-z
    TOKEN_OP, // +,-,*,/,%,**,&,|,^,<<,>>,~
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_COMMA, // ,
    TOKEN_END, 
        
} __attribute__ ((packed)) TokenType;

typedef struct {
    TokenType type;
    union {
        double number;
        char var_name[VAR_NAME_MAX+1];
        OpType op;
        uint8_t func_index;
    } data;
} Token;

typedef struct {
    const char * input;
    size_t cur_pos;
    TokenType prev_token_type;
    char name_buf[VAR_NAME_MAX+1];
} Tokenizer;





bool tokenizer_init(Tokenizer *t, const char *input);

calc_return_t tokenizer_next(Tokenizer *t, Token *out);


