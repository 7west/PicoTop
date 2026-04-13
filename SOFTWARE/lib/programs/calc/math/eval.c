#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>
#include <assert.h>
#include <math.h>

#include "eval.h"
#include "ast.h"
#include "token.h"
#include "vars.h"


#define EVAL_MAX_DEPTH 20
#define ERR_SEND(dst, lit) do {                         \
    _Static_assert(sizeof(lit) <= CALC_ERR_MSG_LEN + 1, \
                "error message too long");              \
    strcpy(dst, lit);                                   \
} while(0)




typedef struct {
    ASTNode nodes[AST_POOL_SIZE];
    uint16_t next_free;
} ASTNodePool;

typedef struct {
    ASTNodePool ast_pool;
    Token tokens[TOKEN_MAX];
    Tokenizer tokenizer;
    ASTNode *node_stack[AST_POOL_SIZE];
    Token *op_stack[TOKEN_MAX];
    int16_t node_index; // write next index
    int16_t op_index; // write next index
    char var_name[VAR_NAME_MAX+1];
    calc_return_t ret_val;
    bool rad_mode;
} Eval_Memory;

static Eval_Memory *emem;


// Static Functions:

static bool parse_to_ast(Token *tokens, ASTNode ** root, char *err_msg);
static ASTNode* ast_alloc_node(ASTNodePool *pool);
static bool separator_pop(TokenType token_type);
static bool push_node_stack(ASTNode *node);
static bool pop_node_stack(ASTNode **node);
static bool push_op_stack(Token *t);
static bool pop_op_stack(void);
static uint8_t get_op_prec(OpType op);
static bool is_left_asc(OpType op);

static calc_return_t ast_eval(ASTNode *node, double *result, uint8_t depth);
static bool is_integer(double val);

static bool parse_assignment(const char *input, char *var_name, const char ** expr_start);
static bool is_alphabetic(const char c);
static void lowercase_buf(char *buf, uint8_t len);
static bool is_numerical(const char c);

static void ast_free_pool(ASTNodePool *pool);

static void calc_error_msg(calc_return_t ret_val, char *err_msg);




size_t eval_mem_size(void) {
    return sizeof(Eval_Memory);
}

void eval_init(void *mem) {
    emem = (Eval_Memory *)mem;

    // other eval init stuff?
}

void calc_eval_reset_ans(void) {
    var_set_ans_valid(true);
    var_set_ans(0);
}


bool calc_eval(const char *input, double *result, char *err_msg, bool rad_mode) {
    ast_free_pool(&emem->ast_pool);

    const char *expr = input;
    bool var_assign = false;
    emem->rad_mode = rad_mode;

    printf("detecting 'let'\n");
    if(parse_assignment(input, emem->var_name, &expr)) {
        if (expr == NULL) { // variable assignment failed
            ERR_SEND(err_msg, "VAR ASSIGN FAIL");
            var_set_ans_valid(false);
            return false;
        }
        var_assign = true;
    }
    printf("initializing tokenizer\n");
    tokenizer_init(&emem->tokenizer, expr);

    uint16_t ti = 0; // token index
    TokenType last_token_type = TOKEN_NULL;

    printf("eating tokens\n");
    do {
        calc_return_t ret_val = tokenizer_next(&emem->tokenizer, &emem->tokens[ti]);
        if (ret_val != CALC_RETURN_SUCCESS) {
            calc_error_msg(ret_val, err_msg);
            var_set_ans_valid(false);
            return false;
        }

        last_token_type = emem->tokens[ti].type;
        ti += 1;
        if (ti == TOKEN_MAX) {
            ERR_SEND(err_msg, "RAN OUT OF TOKENS");
            var_set_ans_valid(false);
            return false;
        }

    } while (last_token_type != TOKEN_END);

    printf("finished tokenizing\n");

    // empty cell. Don't overwrite ans and return
    if (emem->tokens[0].type == TOKEN_END) {
        return true;
    }

    ASTNode *root;
    printf("producing AST...\n");
    if (!parse_to_ast(emem->tokens, &root, err_msg)) {
        var_set_ans_valid(false);
        return false;
    }

    printf("Produced AST\nstarting evaluation...");

    funcs_clear_error();
    uint8_t depth = 0;
    calc_return_t ret_val = ast_eval(root, result, depth);
    if (ret_val != CALC_RETURN_SUCCESS) {
        calc_error_msg(ret_val, err_msg);
        var_set_ans_valid(false);
        return false;
    }

    printf("eval done\n");

    // ast_eval can return a result, but it could be NAN or Inf, so we check
    if (isnan(*result)) {
        if (funcs_get_error() != NULL) {
            memcpy(err_msg, funcs_get_error(), CALC_ERR_MSG_LEN+1);
        } else {
            ERR_SEND(err_msg, "GENERIC MATH ERROR");
        }
        var_set_ans_valid(false);
        return false;
    }
    if (isinf(*result)) {
        calc_error_msg(CALC_INFINITY_RESULT, err_msg);
        var_set_ans_valid(false);
        return false;
    }
    printf("result was valid\n");

    if (var_assign) { // do variable assignment

        printf("assigning var\n");

        if(!vars_set(emem->var_name, *result)) {
            ERR_SEND(err_msg, "VAR ASSIGN FAIL");
            var_set_ans_valid(false);
            return false;
        }
    }

    var_set_ans_valid(true);
    var_set_ans(*result);
    return true;
}

// turn list of tokens into an Abstract Syntax Tree. This one is big.
//  Uses a Op Stack of operators which get popped as needed (precedence, order, special syms, etc)
//  and a Node Stack of ASTNodes. 
//  In the end it returns a single root node to be evaluated
static bool parse_to_ast(Token *tokens, ASTNode ** root, char *err_msg) {

    if (root == NULL || tokens == NULL) {
        ERR_SEND(err_msg, "PARSE INIT FAIL");
        return false;
    }

    uint16_t ti = 0; // token index
    emem->node_index = 0; 
    emem->op_index = 0; 
    
    while (tokens[ti].type != TOKEN_END) {

        switch(tokens[ti].type) {

            case TOKEN_NUM:
            case TOKEN_VAR: {
                ASTNode *temp_node = ast_alloc_node(&emem->ast_pool);
                if (temp_node == NULL) {
                    ERR_SEND(err_msg, "RAN OUT OF NODES");
                    return false;
                }
                if (tokens[ti].type == TOKEN_NUM) {
                    temp_node->type = NODE_NUM;
                    temp_node->data.number = tokens[ti].data.number;
                } else {
                    temp_node->type = NODE_VAR;
                    memcpy(temp_node->data.var_name, tokens[ti].data.var_name, VAR_NAME_MAX+1);
                }
                temp_node->left = NULL;
                temp_node->right = NULL;
                push_node_stack(temp_node);
                break;
            }

            case TOKEN_LPAREN:
            case TOKEN_FUNC:
                push_op_stack(&tokens[ti]);
                break;

            case TOKEN_RPAREN:
            case TOKEN_COMMA:
                if (!separator_pop(tokens[ti].type)) {
                    ERR_SEND(err_msg, "PAREN/COMMA ERROR");
                    return false;
                }
                break;

            case TOKEN_OP:
                
                while (emem->op_index > 0) {
                    Token *top = emem->op_stack[emem->op_index - 1];
                    if (top->type != TOKEN_OP) break; // must end at LPAREN or FUNC

                    OpType incoming_op = tokens[ti].data.op;
                    OpType sitting_op = top->data.op;

                    if ((get_op_prec(sitting_op) > get_op_prec(incoming_op)) 
                    || ((get_op_prec(sitting_op) == get_op_prec(incoming_op)) && (is_left_asc(incoming_op)))) {

                        if (!pop_op_stack()) {
                            ERR_SEND(err_msg, "OP TOKEN SYNTAX ERROR");
                            return false;
                        }
                    } else {
                        break;
                    }
                }

                push_op_stack(&tokens[ti]);
                break;
            default:
                ERR_SEND(err_msg, "UKNOWN TOKEN");
                return false;
        }

        ti += 1;
    }

    // then pop all operators until op_index == 0
    while (emem->op_index > 0) {

        if (emem->op_stack[emem->op_index - 1]->type == TOKEN_LPAREN) {
            ERR_SEND(err_msg, "MISMATCHED PARENS");
            return false;
        }
        if (!pop_op_stack()) {
            ERR_SEND(err_msg, "OP/FUNC SYNTAX ERROR");
            return false;
        }
    }

    if (emem->node_index != 1) { // There should only be one node on the Node stack now at idx 0
        ERR_SEND(err_msg, "UNKNOWN ERROR");
        return false;
    }
    *root = emem->node_stack[0];
    return true;
}

// Commas and Right Parentheses are separators
static bool separator_pop(TokenType token_type) {

    // someone used this function wrong. dummy.
    if (token_type != TOKEN_RPAREN && token_type != TOKEN_COMMA) return false;

    while (emem->op_index > 0 && emem->op_stack[emem->op_index - 1]->type != TOKEN_LPAREN) {
        if (!pop_op_stack()) return false;
    }

    if (emem->op_index == 0) return false; // never found LPAREN. Syntax error

    if (token_type == TOKEN_RPAREN) {
        if(!pop_op_stack()) return false; // to get rid of LPAREN

        if (emem->op_index > 0 && emem->op_stack[emem->op_index - 1]->type == TOKEN_FUNC) {
            if (!pop_op_stack()) return false;
        }
    }

    return true;
}

static bool push_node_stack(ASTNode *node) {
    // this should never happen because ast_allocate_node() has the same pool size
    //      so ignore bool return of this function?
    if (emem->node_index == AST_POOL_SIZE) return false;

    emem->node_stack[emem->node_index] = node;
    emem->node_index += 1;
    return true;
}

static bool pop_node_stack(ASTNode **node) {
    if (emem->node_index == 0) return false;
    emem->node_index -= 1;

    *node = emem->node_stack[emem->node_index];
    return true;
}

static bool push_op_stack(Token *t) {
    // this should never happen because calc_eval() tracks emem->tokens, which
    // has the same pool size as op_stack
    //      so ignore bool return of this function?
    if (emem->op_index == TOKEN_MAX) return false;

    emem->op_stack[emem->op_index] = t;
    emem->op_index += 1;
    return true;
}

// take err_msg for more detailed error returns? maybe one day. It is working great currently
static bool pop_op_stack(void) {

    if (emem->op_index == 0) return false; // there is nothing on the stack
    int16_t op_idx_read = emem->op_index - 1;

    TokenType token_type = emem->op_stack[op_idx_read]->type;

    // if LPAREN: drop, move on
    if (token_type == TOKEN_LPAREN) {
        emem->op_index -= 1;
        return true;
    }
    ASTNode *temp_node = ast_alloc_node(&emem->ast_pool);
    if (temp_node == NULL) return false;

    if (token_type == TOKEN_OP) {

        OpType op_type = emem->op_stack[op_idx_read]->data.op;
        temp_node->data.op = op_type;
        printf("upcoming op type: %d\n", op_type);

        if (op_type == OP_NEGATE || op_type == OP_BIT_NOT) {

            temp_node->type = NODE_UNA_OP;
            if (emem->node_index < 1) return false; // not enough nodes, error
            if (!pop_node_stack(&temp_node->left)) return false;
            temp_node->right = NULL;

        } else {
            temp_node->type = NODE_BIN_OP;
            if (emem->node_index < 2) return false;
            if (!pop_node_stack(&temp_node->right)) return false;
            if (!pop_node_stack(&temp_node->left)) return false;

        }
    } else if (token_type == TOKEN_FUNC) {

        temp_node->type = NODE_FUNC;
        temp_node->data.func_index = emem->op_stack[op_idx_read]->data.func_index;

        FuncEntry *func = funcs_get_func(temp_node->data.func_index);
        if (func == NULL) return false;
        if (func->arg_count == 1) {
            temp_node->right = NULL;
            if (!pop_node_stack(&temp_node->left)) return false;
        } else if (func->arg_count == 2) {
            if (!pop_node_stack(&temp_node->right)) return false;
            if (!pop_node_stack(&temp_node->left)) return false;
        }

    } else { // some weird token got in the Operator Stack
        return false;
    }
    push_node_stack(temp_node);

    emem->op_index -= 1;
    return true;
}


static ASTNode* ast_alloc_node(ASTNodePool *pool) {
    if (pool->next_free == AST_POOL_SIZE) return NULL;

    return &pool->nodes[pool->next_free++];
}

// OLD op_precendence, kept for posterity
// // follows OpType order in ast.h
// static const uint8_t op_precedence[] = {
//     1, // OR
//     2, // XOR
//     3, // AND
//     4, // <<
//     4, // >>
//     5, // ADD
//     5, // SUB
//     6, // MUL
//     6, // DIV
//     6, // MOD
//     7, // NEGATE
//     7, // BIT NOT
//     8, // POW
// };

// follows OpType order in ast.h
static const uint8_t op_precedence[] = {
    1, // OR
    2, // XOR
    3, // AND
    4, // <<
    4, // >>
    5, // ADD
    5, // SUB
    6, // MUL
    6, // DIV
    6, // MOD
    8, // POW
    8, // NEGATE
    8, // BIT NOT
};

static uint8_t get_op_prec(OpType op) {
    return op_precedence[op];
}

// returns if operation is left associative
static bool is_left_asc(OpType op) {
    if (op == OP_NEGATE || op == OP_BIT_NOT || op == OP_POW) return false;
    return true;
}

// Does all the evaluating of nodes. Climbs up the tree recursively
//  Tried to have minimal stack var declaration, thus the depth check
static calc_return_t ast_eval(ASTNode *node, double *result, uint8_t depth) {
    if (node == NULL) return CALC_RETURN_GEN_EVAL_ERROR;
    if (++depth > EVAL_MAX_DEPTH) {
        return CALC_RETURN_EVAL_TOO_DEEP;
    }

    switch (node->type) {
        case NODE_NUM:
            *result = node->data.number;
            return CALC_RETURN_SUCCESS;

        case NODE_VAR:
            if (!vars_get(node->data.var_name, result)) {
                return CALC_RETURN_VAR_GET_ERROR;
            }
            return CALC_RETURN_SUCCESS;

        case NODE_BIN_OP: {
            double left_val, right_val;

            emem->ret_val = ast_eval(node->left, &left_val, depth);
            if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;
            emem->ret_val = ast_eval(node->right, &right_val, depth);
            if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;
            
            switch (node->data.op) {
                case OP_ADD: *result = left_val + right_val; break;
                case OP_SUB: *result = left_val - right_val; break;
                case OP_MUL: *result = left_val * right_val; break;
                case OP_DIV: 
                    if (right_val == 0.0) return CALC_DIV_BY_ZERO;
                    *result = left_val / right_val; 
                    break;
                case OP_MOD: 
                    if (right_val == 0.0) return CALC_DIV_BY_ZERO;
                    *result = fmod(left_val, right_val); 
                    break;
                case OP_POW: 
                    if (left_val == 0.0 && right_val < 0.0) return CALC_INFINITY_RESULT;
                    if (left_val < 0.0 && !is_integer(right_val)) return CALC_IMAGINARY_RESULT;
                    *result = pow(left_val, right_val); 
                    break;
                case OP_BIT_AND: *result = (double)((int64_t)left_val & (int64_t)right_val); break;
                case OP_BIT_OR: *result = (double)((int64_t)left_val | (int64_t)right_val); break;
                case OP_BIT_XOR: *result = (double)((int64_t)left_val ^ (int64_t)right_val); break;
                case OP_SHIFT_L: 
                    if ((int64_t)right_val < 0 || (int64_t)right_val > 63) return CALC_RETURN_BIT_SHIFT_ERROR;
                    *result = (double)((int64_t)left_val << (int64_t)right_val); 
                    break;
                case OP_SHIFT_R: 
                    if ((int64_t)right_val < 0 || (int64_t)right_val > 63) return CALC_RETURN_BIT_SHIFT_ERROR;
                    *result = (double)((int64_t)left_val >> (int64_t)right_val); 
                    break;
                default: return CALC_RETURN_GEN_EVAL_ERROR;
            }
            return CALC_RETURN_SUCCESS;
        }
        case NODE_UNA_OP: {
            double left_val;
            emem->ret_val = ast_eval(node->left, &left_val, depth);
            if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;
            switch (node->data.op) {
                case OP_NEGATE: *result = -left_val; break;
                case OP_BIT_NOT: *result = (double)(~(int64_t)(left_val)); break;
                default: return CALC_RETURN_GEN_EVAL_ERROR;
            }
            return CALC_RETURN_SUCCESS;
        }

        case NODE_FUNC: {
            // Note: the CALC_FUNC_TRIG stuff is to catch trig functions and the deg/rad check
            FuncEntry *func = funcs_get_func(node->data.func_index);
            if (func == NULL) return CALC_FUNCTION_DNE;
            if (func->arg_count == 1) {
                double left_val;
                emem->ret_val = ast_eval(node->left, &left_val, depth);
                if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;

                if (node->data.func_index < CALC_FUNC_TRIG_FUNCS 
                    && !emem->rad_mode) {
                    left_val *= M_PI / 180.0;
                }
                *result = func->func.unary(left_val);

                if ((node->data.func_index >= CALC_FUNC_TRIG_FUNCS 
                    && node->data.func_index < CALC_FUNC_INV_TRIG_FUNCS) 
                    && !emem->rad_mode) {
                    *result *= 180.0 / M_PI;
                }

            } else if (func->arg_count == 2) {
                double left_val, right_val;
                emem->ret_val = ast_eval(node->left, &left_val, depth);
                if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;
                emem->ret_val = ast_eval(node->right, &right_val, depth);
                if (emem->ret_val != CALC_RETURN_SUCCESS) return emem->ret_val;
                *result = func->func.binary(left_val, right_val);

                if ((node->data.func_index >= CALC_FUNC_TRIG_FUNCS 
                    && node->data.func_index < CALC_FUNC_INV_TRIG_FUNCS) 
                    && !emem->rad_mode) {

                    *result *= 180.0 / M_PI;
                }

            } else {
                return CALC_RETURN_FUNC_ARG_COUNT_ERROR;
            }
            return CALC_RETURN_SUCCESS;
            
        }
        default:
            return CALC_RETURN_GEN_EVAL_ERROR;
    }
}

static bool is_integer(double val) {
    return fabs(val - round(val)) < 1e-9;
}


// if parse error occurs, returns true and input == NULL
static bool parse_assignment(const char *input, char *var_name, const char ** expr_start) {

    if (strncmp(input, "let", 3) != 0) return false;

    if (input[3] != ' ' || (!is_alphabetic(input[4]))) { // FAIL
        *expr_start = NULL;
        return true;
    }

    uint8_t var_len = 0;
    while (input[var_len + 1] != '\0' 
        && (is_alphabetic(input[var_len + 4]) || is_numerical(input[var_len + 4]))) {
        
        var_len += 1;
        if (var_len > VAR_NAME_MAX) { //FAIL
            *expr_start = NULL;
            return true;
        }
    }

    memcpy(var_name, input + 4, var_len);
    lowercase_buf(var_name, var_len);
    var_name[var_len] = '\0';

    uint8_t offset = 4 + var_len;
    while (input[offset] != '=') {
        if (input[offset] == '\0' || (input[offset] != ' ')) {
            *expr_start = NULL;
            return true;
        }
        offset += 1;
    }
    offset += 1;
    printf("offset = %u\n", offset);

    *expr_start = input + offset;

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

static bool is_numerical(const char c) {
    if (c >= '0' && c <= '9') return true;
    return false;
}


static void ast_free_pool(ASTNodePool *pool) {
    pool->next_free = 0;
}

// converts the calc_return_t types into strings for the user to read
static void calc_error_msg(calc_return_t ret_val, char *err_msg) {
    switch (ret_val) {

        case CALC_RETURN_NUMBER_PARSE_FAIL:
            ERR_SEND(err_msg, "PARSING NUMBER FAILED");
            break;
        case CALC_RETURN_NAME_PARSE_FAIL:
            ERR_SEND(err_msg, "PARSING NAME FAILED");
            break;
        case CALC_RETURN_OP_PARSE_FAIL:
            ERR_SEND(err_msg, "PARSING OP FAILED");
            break;
        case CALC_RETURN_TOKEN_FAIL:
            ERR_SEND(err_msg, "GENERIC TOKEN FAIL");
            break;
        case CALC_RETURN_UNKNOWN_ERROR:
            ERR_SEND(err_msg, "UNKNOWN ERROR. UH OH");
            break;
        case CALC_RETURN_AST_FAIL:
            ERR_SEND(err_msg, "AST BUILD FAIL");
            break;
        case CALC_RETURN_GEN_EVAL_ERROR:
            ERR_SEND(err_msg, "UNKNOWN EVAL ERROR");
            break;
        case CALC_RETURN_VAR_GET_ERROR:
            ERR_SEND(err_msg, "UNDEFINED VARIABLE");
            break;
        case CALC_RETURN_EVAL_TOO_DEEP:
            ERR_SEND(err_msg, "EXPRESSION TOO COMPLEX");
            break;
        case CALC_DIV_BY_ZERO:
            ERR_SEND(err_msg, "DIV BY 0 ERROR");
            break;
        case CALC_INFINITY_RESULT:
            ERR_SEND(err_msg, "INFINITY RESULT ERROR");
            break;
        case CALC_IMAGINARY_RESULT:
            ERR_SEND(err_msg, "IMAGINARY RESULT ERROR");
            break;
        case CALC_FUNCTION_DNE:
            ERR_SEND(err_msg, "FUNCTION DOES NOT EXIST");
            break;
        case CALC_RETURN_FUNC_ARG_COUNT_ERROR:
            ERR_SEND(err_msg, "FUNCTION ARG COUNT ERR");
            break;
        case CALC_RETURN_BIT_SHIFT_ERROR:
            ERR_SEND(err_msg, "BIT SHIFT AMOUNT ERROR");
            break;
        



        default:
            ERR_SEND(err_msg, "UNKNOWN ERROR. UH OH");

    }
}

