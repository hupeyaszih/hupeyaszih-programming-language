#include "core/parser.h"
#include "core/globals.h"
#include "core/lexer.h"
#include "core/symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static inline int is_boolean_logic_token(enum token_type type){ // returns token ID 
    switch (type) {
        case LEXER_TOKEN_TYPE_EQUAL_EQUAL: return 1;
        case LEXER_TOKEN_TYPE_BANG_EQUAL: return 2;

        case LEXER_TOKEN_TYPE_LESS_EQUAL: return 3;
        case LEXER_TOKEN_TYPE_GREATER_EQUAL: return 4;
        case LEXER_TOKEN_TYPE_LESS: return 5;
        case LEXER_TOKEN_TYPE_GREATER: return 6;
        default: return 0;
    }

    return 0; // False
}

struct parser_t *parser_create_parser(){
    struct parser_t *parser = calloc(1, sizeof(struct parser_t));
    if(!parser) {C_LOG_ERR("parser_create_parser - couldn't create parser"); free(parser); return NULL;}
    parser->node_count = 0;
    parser->scope_counter = 0;
    parser->loop_depth_counter = 0;
    parser->loop_id_counter = 0;
    parser->loop_control_depth_counter = 0;
    parser->loop_control_id_counter = 0;
    parser->current_loop_id = 0;
    parser->successful = 1;

    return parser;
}
void parser_delete_parser(struct parser_t **parser){
    if(NULL == parser || NULL == *parser) {C_LOG_ERR("parser_delete_parser - \"struct parser_t **parser\" or \"*parser\" is null"); return;}
    for (int i = 0; i < (*parser)->node_count; i++) {
        if ((*parser)->nodes[i]) {
            parser_delete_node(&((*parser)->nodes[i]));
        }
    }
    free((*parser)->nodes);
    free((*parser));
    *parser = NULL;
}

struct parser_node *parser_create_node(enum parser_node_type type, int line){
    struct parser_node *node = calloc(1, sizeof(struct parser_node));
    node->type = type;
    node->line = line;
    node->is_literal_data_created_by_parser = 0;

    return node;
}
void parser_delete_node(struct parser_node **node) {
    if (NULL == node || NULL == (*node)) return;

    if ((*node)->left_node) parser_delete_node(&((*node)->left_node));
    if ((*node)->right_node) parser_delete_node(&((*node)->right_node));

    switch ((*node)->type) {
        case PARSER_NODE_BLOCK:
            for (int i = 0; i < (*node)->data.block.count; i++) {
                parser_delete_node(&((*node)->data.block.statements[i]));
            }
            free((*node)->data.block.statements);
            if ((*node)->data.block.scope) {
                symbol_table_delete_symbol_table(&((*node)->data.block.scope));
            }
            break;

        case PARSER_NODE_FUNCTION:
            free((*node)->data.function.name);
            free((*node)->data.function.mangled_name);
            if ((*node)->data.function.params) {
                parser_delete_node(&((*node)->data.function.params));
            }
            if ((*node)->data.function.body) {
                parser_delete_node(&((*node)->data.function.body));
            }
            break;
            
        case PARSER_NODE_CALL:
            free((*node)->data.call.name);
            for (int i = 0; i < (*node)->data.call.arg_count; i++) {
                parser_delete_node(&((*node)->data.call.args[i]));
            }
            free((*node)->data.call.args);
            break;
        case PARSER_NODE_LOOP:
            parser_delete_node(&((*node)->data.loop.body));
            free((*node)->data.loop.mangled_name);
            break;
        case PARSER_NODE_BREAK:
            parser_delete_node(&((*node)->data.loop_control.condition));
            parser_delete_node(&((*node)->data.loop_control.body));
            free((*node)->data.loop_control.mangled_loop_control_name);
            free((*node)->data.loop_control.mangled_loop_name);
            break;
        case PARSER_NODE_CONTINUE:
            parser_delete_node(&((*node)->data.loop_control.condition));
            parser_delete_node(&((*node)->data.loop_control.body));
            free((*node)->data.loop_control.mangled_loop_control_name);
            free((*node)->data.loop_control.mangled_loop_name);
            break;
        case PARSER_NODE_ASM:
            free((*node)->data.assembly.assembly_data);
            break;
        case PARSER_NODE_VARIABLE_DECLARATION:
            if ((*node)->data.variable_name != NULL) {
                free((*node)->data.variable_name);
                (*node)->data.variable_name = NULL; 
            }
            break;
        default: break;
    }

    if(1 == (*node)->is_literal_data_created_by_parser) {
        free((*node)->data.literal_data);
    }
    free(*node);
    *node = NULL;
}

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node){
    if(NULL == node) {LOG_M_ERR("parser_parser_add_node - \"struct parser_node *node\" is null"); return;}
    if(NULL == parser) {LOG_M_ERR("parser_parser_add_node - \"struct parser_t *parser\" is null"); return;}

    struct parser_node **tmp = realloc(parser->nodes, sizeof(struct parser_node *) * (parser->node_count+1));
    if(!tmp) {LOG_M_ERR("parser_parser_add_node - couldn't realloc \"parser->nodes\""); return;}
    tmp[parser->node_count] = node;
    parser->nodes = tmp;
    parser->node_count += 1;
    return;
}


static inline struct lexer_token* eat(struct lexer_token *tokens, int token_count, int *cursor, enum token_type expected_type) {
    if (*cursor >= token_count || tokens[*cursor].type != expected_type) {
        if (*cursor >= token_count) {C_LOG_ERR("unexpected token type on line %d", tokens[*cursor].line);}
        else {C_LOG_ERR("expected token type (\"%s\") instead of token type (\"%s\") on line %d", lexer_token_type_to_string(expected_type) ,lexer_token_type_to_string(tokens[*cursor].type),tokens[*cursor].line);}
        (*cursor)++;
        return NULL;
    }
    return &tokens[(*cursor)++];
}

static inline void parser_synchronize(struct lexer_token *tokens, int token_count, int *cursor) {
    if (*cursor < token_count) {
        (*cursor)++;
    }

    while (*cursor < token_count) {
        if (tokens[*cursor].type == LEXER_TOKEN_TYPE_SEMICOLON) {
            (*cursor)++;
            return;
        }

        switch (tokens[*cursor].type) {
            case LEXER_TOKEN_TYPE_FN:
            case LEXER_TOKEN_TYPE_VAR:
            case LEXER_TOKEN_TYPE_LBRACE:
            case LEXER_TOKEN_TYPE_RBRACE:
                return; 
            default:
                (*cursor)++;
                break;
        }
    }
}
int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file) {
    int cursor = 0;
    while (cursor < file->token_count) {
        struct parser_node *node = parser_parse_statement(parser, file->tokens, file->token_count, &cursor);
        if (node) {
            parser_parser_add_node(parser, node);
        } else {
            parser->successful = 0;
            parser_synchronize(file->tokens, file->token_count, &cursor);
        }
    }
    return parser->successful;
}

struct parser_node *parser_parse_variable_declaration(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_variable_declaration - \"struct parser_t *restrict parser\" is null"); return NULL;}

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_VAR)) {parser->successful = 0; return NULL;}

    if (*cursor >= token_count) {parser->successful = 0;return NULL;}
    struct lexer_token name_token = tokens[*cursor];
    char *var_name = tokens[*cursor].token;

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser->successful = 0; return NULL;}

    char *type_name = NULL;
    if(NULL != eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON)) {

        struct lexer_token *type_name_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER); 
        if(NULL == type_name_token) {parser->successful = 0; return NULL;}

        type_name = type_name_token->token;
        if(NULL == type_name) {parser->successful = 0; return NULL;}
    }else {
        parser->successful = 0;
        return NULL;
    }

    struct type_info *type_info = type_table_get_type_info(parser->type_table, type_name);
    if(NULL == type_info) {
        LOG_M_ERR("parser_parse_variable_declaration - \"struct type_info *type_info\" is null");
        parser->successful = 0;
        return NULL;
    }

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL)) {parser->successful = 0; return NULL;}

    struct parser_node *value_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == value_node) {parser->successful = 0; return NULL;}
    struct parser_node *decl_node = parser_create_node(PARSER_NODE_VARIABLE_DECLARATION, name_token.line);
    if(NULL == decl_node) {parser_delete_node(&value_node); parser->successful = 0; return NULL;}

    decl_node->data.variable_name = strdup(var_name);
    if(NULL == decl_node->data.variable_name) {
        parser_delete_node(&decl_node);
        parser_delete_node(&value_node);
        parser->successful = 0;
        return NULL;
    }
    
    decl_node->right_node = value_node; 
    if(NULL == symbol_table_define(parser->current_scope, var_name, type_info, SYMBOL_KIND_VARIABLE)) {
        parser_delete_node(&decl_node);
        parser->successful = 0;
        return NULL;
    }

    return decl_node;
}

struct parser_node *parser_parse_statement(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_RBRACE) {
        parser->successful = 0;
        return NULL; 
    }
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_LBRACE) {
        return parser_parse_block(parser, tokens, token_count, cursor, 1);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_FN){
        return parser_parse_function(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_LOOP){
        return parser_parse_loop(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_BREAK){
        return parser_parse_break(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_CONTINUE){
        return parser_parse_continue(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_ASM){
        return parser_parse_asm(parser, tokens, token_count, cursor);
    }

    struct parser_node *node = NULL;
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_VAR) {
        node = parser_parse_variable_declaration(parser, tokens, token_count, cursor);
    }else {
        node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    }

    if (NULL == node) {parser->successful = 0; return NULL;}

    if (node->type != PARSER_NODE_BLOCK && node->type != PARSER_NODE_FUNCTION && node->type != PARSER_NODE_LOOP && node->type != PARSER_NODE_BREAK && node->type != PARSER_NODE_CONTINUE) {
        if (NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_SEMICOLON)) {
            parser->successful = 0;
            parser_delete_node(&node);
            return NULL;
        }
    }

    return node;
}

struct parser_node *parser_parse_assignment(struct parser_t *parser, struct lexer_token *tokens, int token_count, int *cursor) {
    int line = tokens[*cursor].line;

    struct parser_node *left_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == left_node) {
        parser->successful = 0;
        return NULL;
    }

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL)) {
        parser_delete_node(&left_node);
        parser->successful = 0; 
        return NULL;
    }

    struct parser_node *right_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == right_node) {
        parser_delete_node(&left_node);
        parser->successful = 0;
        return NULL;
    }

    struct parser_node *node = parser_create_node(PARSER_NODE_VARIABLE_ASSIGMENT, line);
    node->left_node = left_node;
    node->right_node = right_node;
    
    node->type_info = left_node->type_info;

    return node;
}

struct parser_node *parser_parse_call(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, char *func_name) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_call - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *call_node = parser_create_node(PARSER_NODE_CALL, tokens[*cursor].line);
    if(NULL == call_node){
        LOG_M_ERR("parser_parse_call - \"struct parser_node *call_node\" is null");
        parser->successful = 0;
        return NULL;
    }

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {
        parser->successful = 0; 
        parser_delete_node(&call_node);
        return NULL;
    }

    call_node->data.call.name = strdup(func_name);
    int capacity = 4;
    call_node->data.call.args = malloc(sizeof(struct parser_node*) * capacity);
    call_node->data.call.arg_count = 0;

    if (!call_node->data.call.name || !call_node->data.call.args) {
        parser_delete_node(&call_node);
        parser->successful = 0;
        return NULL;
    }

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RPAREN) {
        if (call_node->data.call.arg_count >= capacity) {
            capacity *= 2;
            struct parser_node **tmp = realloc(call_node->data.call.args, sizeof(struct parser_node*) * capacity);
            if (!tmp) {
                parser_delete_node(&call_node);
                parser->successful = 0;
                return NULL;
            }
            call_node->data.call.args = tmp;
        }

        struct parser_node *arg = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        if(NULL == arg){
            parser_delete_node(&call_node);
            parser->successful = 0;
            return NULL;
        }
        call_node->data.call.args[call_node->data.call.arg_count++] = arg;

        if (*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_COMMA) {
            if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COMMA)) {
                parser_delete_node(&call_node);
                parser->successful = 0;
                return NULL;
            }
        }
    }

    if(*cursor >= token_count || NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {
        parser_delete_node(&call_node);
        parser->successful = 0;
        return NULL;
    }

    return call_node;
}

struct parser_node *parser_parse_parameters(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_parameters - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *params_node = parser_create_node(PARSER_NODE_BLOCK, tokens[*cursor].line);
    if(NULL == params_node){
        parser_delete_node(&params_node);
        LOG_M_ERR("parser_parse_parameters - \"struct parser_node *params_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    int capacity = 4;
    params_node->data.block.statements = malloc(sizeof(struct parser_node*) * capacity);
    params_node->data.block.count = 0;

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RPAREN) {
        struct parser_node *p_node = parser_create_node(PARSER_NODE_VARIABLE_DECLARATION, tokens[*cursor].line);
        p_node->left_node = NULL;
        p_node->right_node = NULL;

        p_node->data.variable_name = strdup(tokens[*cursor].token);
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {
            parser_delete_node(&params_node);
            parser_delete_node(&p_node);
            parser->successful = 0;
            return NULL;
        }


        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON)) {
            parser_delete_node(&params_node);
            parser_delete_node(&p_node);
            parser->successful = 0;
            return NULL;
        }

        p_node->type_info = type_table_get_type_info(parser->type_table, tokens[*cursor].token);
        if(NULL == p_node->type_info) {
            C_LOG_ERR("Unknown parameter type on line: %d", tokens[*cursor].line);
            parser_delete_node(&params_node);
            parser_delete_node(&p_node);
            parser->successful = 0;
            return NULL;
        }
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {
            parser_delete_node(&params_node);
            parser_delete_node(&p_node);
            parser->successful = 0;
            return NULL;
        }

        if (params_node->data.block.count >= capacity) {
            capacity *= 2;
            params_node->data.block.statements = realloc(params_node->data.block.statements, sizeof(struct parser_node*) * capacity);
        }
        params_node->data.block.statements[params_node->data.block.count++] = p_node;

        if (*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_COMMA) {
            if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COMMA)) {
                parser_delete_node(&params_node);
                parser->successful = 0;
                return NULL;
            }

            if (tokens[*cursor].type == LEXER_TOKEN_TYPE_RPAREN) {
                parser_delete_node(&params_node);
                C_LOG_ERR("Trailing comma in parameters is not allowed!");
                parser->successful = 0;
                return NULL;
            }
        }
    }

    return params_node;
}

struct parser_node *parser_parse_loop(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_loop - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *loop_node = parser_create_node(PARSER_NODE_LOOP, tokens[*cursor].line);
    if(NULL == loop_node){
        LOG_M_ERR("parser_parse_loop - \"struct parser_node *loop_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LOOP)) goto cleanup_err_level_0;
    parser->loop_depth_counter++;
    parser->loop_id_counter++;
    loop_node->data.loop.loop_id = parser->loop_id_counter;

    int old_current_loop_id = parser->current_loop_id;
    parser->current_loop_id = loop_node->data.loop.loop_id;

    struct parser_node *loop_body = parser_parse_block(parser, tokens, token_count, cursor, 1);
    if(NULL == loop_body) goto cleanup_err_level_0;
    loop_node->data.loop.body = loop_body;

    parser->loop_depth_counter--;
    parser->current_loop_id = old_current_loop_id;
    return loop_node;

cleanup_err_level_0:
    parser_delete_node(&loop_node);
    parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_break(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_break - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *break_node = parser_create_node(PARSER_NODE_BREAK, tokens[*cursor].line);
    if(NULL == break_node){
        LOG_M_ERR("parser_parse_break - \"struct parser_node *break_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    if(parser->loop_depth_counter <= 0) {C_LOG_ERR("\"break\" can only be used in loops"); goto cleanup_err_level_0;}
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_BREAK)) goto cleanup_err_level_0;
    parser->loop_control_id_counter++;
    parser->loop_control_depth_counter++;
    break_node->data.loop_control.loop_control_id = parser->loop_control_id_counter;
    break_node->data.loop_control.loop_id = parser->current_loop_id;

    struct parser_node *condition_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == condition_node) goto cleanup_err_level_0;
    break_node->data.loop_control.condition = condition_node;

    struct parser_node *body_node = parser_parse_block(parser, tokens, token_count, cursor, 1);
    if(NULL == body_node) goto cleanup_err_level_0;
    break_node->data.loop_control.body = body_node;


    parser->loop_control_depth_counter--;
    return break_node;
cleanup_err_level_0:
    parser->loop_control_depth_counter--;
    parser_delete_node(&break_node);
    parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_continue(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_continue - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *continue_node = parser_create_node(PARSER_NODE_CONTINUE, tokens[*cursor].line);
    if(NULL == continue_node){
        LOG_M_ERR("parser_parse_continue - \"struct parser_node *continue_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_CONTINUE)) goto cleanup_err_level_0;
    parser->loop_control_id_counter++;
    parser->loop_control_depth_counter++;
    continue_node->data.loop_control.loop_control_id = parser->loop_control_id_counter;
    continue_node->data.loop_control.loop_id = parser->current_loop_id;

    struct parser_node *condition_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == condition_node) goto cleanup_err_level_0;
    continue_node->data.loop_control.condition = condition_node;

    struct parser_node *body_node = parser_parse_block(parser, tokens, token_count, cursor, 1);
    if(NULL == body_node) goto cleanup_err_level_0;
    continue_node->data.loop_control.body = body_node;


    parser->loop_control_depth_counter--;
    return continue_node;
cleanup_err_level_0:
    parser->loop_control_depth_counter--;
    parser_delete_node(&continue_node);
    parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_asm(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){

    if(NULL == parser) {LOG_M_ERR("parser_parse_asm - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *asm_node = parser_create_node(PARSER_NODE_ASM, tokens[*cursor].line);
    if(NULL == asm_node){
        LOG_M_ERR("parser_parse_asm - \"struct parser_node *continue_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_ASM)) goto cleanup_err_level_0;

    struct lexer_token *assembly_data = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_STRING_LITERAL);
    if(NULL == assembly_data) {C_LOG_ERR("String Literal (assembly) expected, line %d", asm_node->line); goto cleanup_err_level_0;}
    char *ptr = assembly_data->token;
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') {
        ptr++;
    }
    asm_node->data.assembly.assembly_data = strdup(assembly_data->token);

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_SEMICOLON)) {C_LOG_ERR("expected \";\" on line %d", asm_node->line); goto cleanup_err_level_0;}

    return asm_node;
cleanup_err_level_0:
    parser_delete_node(&asm_node);
    parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_function(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_function - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *function_node = parser_create_node(PARSER_NODE_FUNCTION, tokens[*cursor].line);
    if(NULL == function_node){
        LOG_M_ERR("parser_parse_function - \"struct parser_node *function_node\" is null");
        parser->successful = 0;
        return NULL;
    }

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_FN)) {parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    char *name = tokens[*cursor].token; 
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    struct parser_node *parameters = parser_parse_parameters(parser, tokens, token_count, cursor);
    if(NULL == parameters || NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0;return NULL;}

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON)) {parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    char *return_type_name = tokens[*cursor].token;
    struct type_info *ret_type = type_table_get_type_info(parser->type_table, return_type_name);
    if(NULL == ret_type) {C_LOG_ERR("Unknown return type for function on line: %d", tokens[*cursor].line);parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0; return NULL;}
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0;return NULL;}

    struct symbol_table *body_scope = symbol_table_create_symbol_table(parser->current_scope, &parser->scope_counter);
    if(NULL == body_scope){
        parser_delete_node(&parameters);
        parser_delete_node(&function_node); 
        parser->successful = 0;
        return NULL;
    }
    
    for(int i = 0; i < parameters->data.block.count; i++) {
        struct parser_node *p = parameters->data.block.statements[i];
        if(NULL == symbol_table_define(body_scope, p->data.variable_name, p->type_info, SYMBOL_KIND_VARIABLE)) {symbol_table_delete_symbol_table(&body_scope); parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    }

    struct symbol_table *old_scope = parser->current_scope;
    parser->current_scope = body_scope;

    function_node->data.function.body = parser_parse_block(parser, tokens, token_count, cursor, 0);
    if(NULL == function_node->data.function.body) {
        parser_delete_node(&parameters);
        parser_delete_node(&function_node);
        parser->current_scope = old_scope;
        parser->successful = 0;
        return NULL;
    }
    
    parser->current_scope = old_scope;

    function_node->data.function.params = parameters;
    function_node->data.function.name = strdup(name);
    function_node->data.function.mangled_name = strdup(name);
    function_node->data.function.return_type = ret_type;
    function_node->data.function.param_count = parameters->data.block.count;

    if(NULL == symbol_table_define(parser->current_scope, function_node->data.function.name, type_table_get_type_info(parser->type_table, "fn"), SYMBOL_KIND_FUNCTION)) {symbol_table_delete_symbol_table(&body_scope); parser_delete_node(&parameters); parser_delete_node(&function_node); parser->successful = 0;return NULL;}
    return function_node;
}

struct parser_node *parser_parse_block(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int create_new_scope) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_block - \"struct parser_t *restrict parser\" is null"); return NULL;}
    if (*cursor >= token_count) {parser->successful = 0; return NULL;}
    int line_number = tokens[*cursor].line;
    struct parser_node *block_node = parser_create_node(PARSER_NODE_BLOCK, line_number);
    if(NULL == block_node){
        parser_delete_node(&block_node);
        LOG_M_ERR("parser_parse_block - \"struct parser_node *block_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    
    if(1 == create_new_scope) {
        parser->current_scope = symbol_table_create_symbol_table(parser->current_scope, &parser->scope_counter);
    }
    block_node->data.block.scope = parser->current_scope;

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LBRACE)) {
        parser_delete_node(&block_node);
        if(1 == create_new_scope) parser->current_scope = parser->current_scope->parent;
        parser->successful = 0;
        return NULL;
    }

    int capacity = 16;
    block_node->data.block.statements = calloc(capacity, sizeof(struct parser_node*)); 
    block_node->data.block.count = 0;

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RBRACE) {
        struct parser_node *stmt = parser_parse_statement(parser, tokens, token_count, cursor);
        if(NULL == stmt) {
            parser_synchronize(tokens, token_count, cursor);
            if (*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_RBRACE) {
                parser->successful = 0;
                break;
            }
        }

        if (block_node->data.block.count >= capacity) {
            capacity *= 2;
            struct parser_node **tmp = realloc(block_node->data.block.statements, sizeof(struct parser_node*) * capacity);
            if (!tmp) {
                parser_delete_node(&block_node);
                if(1 == create_new_scope) parser->current_scope = parser->current_scope->parent;
                parser->successful = 0;
                return NULL;
            }
            block_node->data.block.statements = tmp;
        }
        block_node->data.block.statements[block_node->data.block.count++] = stmt;
    }

    if (*cursor >= token_count || tokens[*cursor].type != LEXER_TOKEN_TYPE_RBRACE) {
        C_LOG_ERR("expected '}' to close block starting on line: %d", line_number);
        if(1 == create_new_scope) parser->current_scope = parser->current_scope->parent;
        parser->successful = 0;
        return block_node; 
    }
    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RBRACE);

    if(1 == create_new_scope){
        parser->current_scope = parser->current_scope->parent;
    }

    return block_node;
}

struct parser_node *parser_parse_boolean_logic(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_expression(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }

    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_EQUAL) {
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL); 
        struct parser_node *right = parser_parse_expression(parser, tokens, token_count, cursor);
        
        struct parser_node *node = parser_create_node(PARSER_NODE_VARIABLE_ASSIGMENT, tokens[*cursor].line);
        node->left_node = left;
        node->right_node = right;
        node->type_info = left->type_info;
        return node;
    }

    int node_type_id = is_boolean_logic_token(tokens[*cursor].type);
    while(*cursor < token_count && node_type_id != 0) {
        int op_line = tokens[*cursor].line;
        enum parser_node_type op_type = node_type_id;
        if(NULL == eat(tokens, token_count, cursor, tokens[*cursor].type)) {
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *right = parser_parse_expression(parser, tokens, token_count, cursor);
        if(NULL == right){
            C_LOG_ERR("Expected expression after boolean operator on line %d", op_line);
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, op_line);
        if(NULL == new_node) {
            parser_delete_node(&left);
            parser_delete_node(&right);
            parser->successful = 0;
            return NULL;
        }
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

        if (*cursor < token_count) {
            node_type_id = is_boolean_logic_token(tokens[*cursor].type);
        } else {
            node_type_id = 0;
        }
    }
    return left;
}

struct parser_node *parser_parse_expression(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_expression - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_term(parser, tokens, token_count, cursor);
    if(NULL == left){
        LOG_M_ERR("parser_parse_expression - \"struct parser_node *left\" is null %d", tokens[*cursor].line);
        parser->successful = 0;
        return NULL;
    }

    while(*cursor < token_count && (LEXER_TOKEN_TYPE_PLUS == tokens[*cursor].type || LEXER_TOKEN_TYPE_MINUS == tokens[*cursor].type)) {
        enum parser_node_type op_type = PARSER_NODE_MINUS;
        int op_line = tokens[*cursor].line;
        if(LEXER_TOKEN_TYPE_PLUS == tokens[*cursor].type) op_type = PARSER_NODE_PLUS;
        if(NULL == eat(tokens, token_count, cursor, tokens[*cursor].type)) {
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *right = parser_parse_term(parser, tokens, token_count, cursor);
        if(!right){
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, op_line);
        if(NULL == new_node) {
            parser_delete_node(&left);
            parser_delete_node(&right);
            parser->successful = 0;
            return NULL;
        }
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_term(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_term - \"struct parser_t *restrict parser\" is null"); return NULL;}

    struct parser_node *left = parser_parse_unary(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }

    while(*cursor < token_count && (LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type || LEXER_TOKEN_TYPE_SLASH == tokens[*cursor].type)) {
        enum parser_node_type op_type = PARSER_NODE_DIVIDE;
        if(LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type) op_type = PARSER_NODE_MUL;
        if(NULL == eat(tokens, token_count, cursor, tokens[*cursor].type)) {
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *right = parser_parse_unary(parser, tokens, token_count, cursor);
        if(NULL == right){
            parser_delete_node(&left);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
        if(NULL == new_node) {
            parser_delete_node(&left);
            parser_delete_node(&right);
            parser->successful = 0;
            return NULL;
        }
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

    }
    return left;
}
struct parser_node *parser_parse_factor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_factor - \"struct parser_t *restrict parser\" is null"); return NULL;}
    if(*cursor >= token_count) {
        LOG_M_ERR("parser_parse_factor - \"*cursor >= token_count\"");
        parser->successful = 0;
        return NULL;
    }

    if(LEXER_TOKEN_TYPE_INT_LITERAL == tokens[*cursor].type) {
        int line_number = tokens[*cursor].line;
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_INT_LITERAL);

        if(NULL == t) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.literal_data = t->token;
        return node;
    }else if(LEXER_TOKEN_TYPE_IDENTIFIER == tokens[*cursor].type){
        if((*cursor)+1 < token_count && tokens[(*cursor)+1].type == LEXER_TOKEN_TYPE_LPAREN) {
            char *name_to_pass = tokens[*cursor].token;
            if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser->successful = 0; return NULL;} 
            return parser_parse_call(parser, tokens, token_count, cursor, name_to_pass);
        }
        int line_number = tokens[*cursor].line;
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        if(NULL == t || NULL == t->token) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(PARSER_NODE_IDENTIFIER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.variable_name = t->token;

        struct symbol_t *sym = symbol_table_look_up(parser->current_scope, node->data.variable_name);
        if(NULL == sym || NULL == sym->type){
            C_LOG_ERR("undefined variable (%s), line: %d", node->data.variable_name, node->line);
            parser->successful = 0;
            parser_delete_node(&node);
            return NULL;
        }
        node->type_info = sym->type;
        return node;
    }else if(LEXER_TOKEN_TYPE_LPAREN == tokens[*cursor].type){
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {parser->successful = 0; return NULL;}

        struct parser_node *node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        if(NULL == node) {parser->successful = 0; return NULL;}
        if(*cursor >= token_count) {parser_delete_node(&node); parser->successful = 0; return NULL;}
        int line_number = tokens[*cursor].line;

        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)){
            parser_delete_node(&node);
            C_LOG_ERR("expected \")\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        return node;
    }else if(LEXER_TOKEN_TYPE_SIZEOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_SIZEOF)) {
            parser->successful = 0;
            return NULL;
        }
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {
            C_LOG_ERR("expected \"(\" for \"sizeof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        if(NULL == type_token) {
            C_LOG_ERR("expected an identifier for \"sizeof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct type_info *type_info = type_table_get_type_info(parser->type_table, type_token->token);
        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"sizeof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {
            C_LOG_ERR("expected \")\" for \"sizeof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", type_info->size);
        node->data.literal_data = strdup(buf);
        return node;
    }else if(LEXER_TOKEN_TYPE_ALIGNOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_ALIGNOF)) {
            parser->successful = 0;
            return NULL;
        }
        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {
            C_LOG_ERR("expected \"(\" for \"alignof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        if(NULL == type_token) {
            C_LOG_ERR("expected an identifier for \"alignof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct type_info *type_info = type_table_get_type_info(parser->type_table, type_token->token);
        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"alignof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {
            C_LOG_ERR("expected \")\" for \"alignof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", type_table_size_padding(type_info->size));
        node->data.literal_data = strdup(buf);
        return node;
    }else {
        C_LOG_ERR("parser_parse_factor - current token (%s) is not literal or identifier (unexpected token), line: %d", tokens[*cursor].token ,tokens[*cursor].line);
        (*cursor)++;
        parser->successful = 0;
        return NULL;
    }
    // parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(*cursor >= token_count) {LOG_M_ERR("parser_parse_unary - \"*cursor >= token_count\""); parser->successful = 0;return NULL;}
    enum token_type tok_type = tokens[*cursor].type;
    if (LEXER_TOKEN_TYPE_MINUS == tok_type || LEXER_TOKEN_TYPE_PLUS == tok_type || LEXER_TOKEN_TYPE_BANG == tok_type || LEXER_TOKEN_TYPE_STAR == tok_type || LEXER_TOKEN_TYPE_AMPERSAND == tok_type) {
        int op_line = tokens[*cursor].line;
        struct lexer_token *op_token = NULL;
        enum parser_node_type parser_node_type;

        if(LEXER_TOKEN_TYPE_PLUS == tok_type) {parser_node_type = PARSER_NODE_UNDEFINED; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_PLUS);}
        else if(LEXER_TOKEN_TYPE_MINUS == tok_type) {parser_node_type = PARSER_NODE_UNARY_MINUS; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_MINUS);}
        else if(LEXER_TOKEN_TYPE_BANG == tok_type) {parser_node_type = PARSER_NODE_UNARY_BANG; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_BANG);}
        else if(LEXER_TOKEN_TYPE_STAR == tok_type) {parser_node_type = PARSER_NODE_UNARY_DEREFERENCE; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_STAR);}
        else if(LEXER_TOKEN_TYPE_AMPERSAND == tok_type) {parser_node_type = PARSER_NODE_UNARY_ADDRESS_OF; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_AMPERSAND);}
        if(NULL == op_token) {parser->successful = 0; return NULL;}

        struct parser_node *right_node = parser_parse_unary(parser, tokens, token_count, cursor);
        if(NULL == right_node) {parser->successful = 0; return NULL;}
        

        if (LEXER_TOKEN_TYPE_PLUS == op_token->type) {
            return right_node; 
        }

        struct parser_node *node = parser_create_node(parser_node_type, op_line);
        if(NULL == node) {
            parser_delete_node(&right_node);
            parser->successful = 0;
            return NULL;
        }

        node->right_node = right_node;
        
        return node;
    }

    return parser_parse_factor(parser, tokens, token_count, cursor);
}


int parser_parse_control_depth(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int cursor){
    int depth = 0;
    int max_depth = 0;
    while(cursor < token_count){
        enum token_type current_type = tokens[cursor].type;
        if(LEXER_TOKEN_TYPE_LPAREN == current_type) {depth++;}
        else if(LEXER_TOKEN_TYPE_RPAREN == current_type) {depth--;}

        if(depth < 0) return -1;

        cursor++;
    }
    return 0;
}
