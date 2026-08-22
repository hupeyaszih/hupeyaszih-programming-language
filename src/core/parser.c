#include "core/parser.h"
#include "core/flags/function_flags.h"
#include "core/globals.h"
#include "core/lexer.h"
#include "core/symbol_table.h"
#include "h_bitset.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline struct str_view parser_block_generate_mangled_name(struct parser_t *parser, struct parser_node *block) {
    char scope_id[32];
    snprintf(scope_id, sizeof(scope_id), "%d", parser->current_scope->scope_id);

    char id[32];
    snprintf(id, sizeof(id), "%d", block->data.block.id);
    
    size_t size = strlen(id) + strlen(scope_id) + 4;
    
    char *mangled_name = arena_alloc(parser->arena, sizeof(char) * size);
    if (!mangled_name) {
        return (struct str_view){ NULL, 0 };
    }

    snprintf(mangled_name, size, "B_%s_%s", scope_id, id);

    return str_view_make(mangled_name, size);
}
static inline struct str_view parser_function_generate_mangled_name(struct parser_t *parser, struct parser_node *function) {
    char scope_id[32];
    snprintf(scope_id, sizeof(scope_id), "%d", parser->current_scope->scope_id);
    
    size_t name_len = function->data.function.name.len + strlen(scope_id) + 3;
    size_t alloc_size = name_len + 1;
    
    char *mangled_name = arena_alloc(parser->arena, alloc_size * sizeof(char));
    if (!mangled_name) {
        return (struct str_view){ NULL, 0 };
    }

    snprintf(mangled_name, alloc_size, "F_%s_" SV_FMT, scope_id, SV_ARG(function->data.function.name));

    return str_view_make(mangled_name, name_len);
}

static inline int calculate_pointer_level(struct lexer_token *tokens, int *cursor) {
    int pointer_level = 0;
    while(LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type) {
        ++pointer_level;
        ++(*cursor);
    }

    return pointer_level;
}

static inline int is_boolean_logic_token(enum token_type type){ // returns token ID 
    switch (type) {
        case LEXER_TOKEN_TYPE_EQUAL_EQUAL: return PARSER_NODE_EQUAL_EQUAL;
        case LEXER_TOKEN_TYPE_BANG_EQUAL: return PARSER_NODE_BANG_EQUAL;

        case LEXER_TOKEN_TYPE_LESS_EQUAL: return PARSER_NODE_LESS_EQUAL;
        case LEXER_TOKEN_TYPE_GREATER_EQUAL: return PARSER_NODE_GREATER_EQUAL;
        case LEXER_TOKEN_TYPE_LESS: return PARSER_NODE_LESS;
        case LEXER_TOKEN_TYPE_GREATER: return PARSER_NODE_GREATER;
        default: return -1;
    }

    return -1; // False
}

static inline int is_shift_operator_token(enum token_type type){
    switch (type) {
        case LEXER_TOKEN_TYPE_SHL: return PARSER_NODE_SHL;
        case LEXER_TOKEN_TYPE_SHR: return PARSER_NODE_SHR;
        default: return -1;
    }
    return -1;
}

struct parser_t *parser_create_parser(struct arena *arena, struct arena *symbol_arena){
    struct parser_t *parser = arena_alloc(arena, sizeof(struct parser_t));
    if(!parser) {C_LOG_ERR("parser_create_parser - couldn't create parser");return NULL;}
    parser->arena = arena;
    parser->symbol_arena = symbol_arena;
    parser->nodes = vector_create_vector(arena, 4, sizeof(struct parser_node *));
    parser->block_counter = 0;
    parser->scope_counter = 0;
    parser->loop_depth_counter = 0;
    parser->loop_id_counter = 0;
    parser->current_loop_id = 0;
    parser->successful = 1;

    return parser;
}

struct parser_node *parser_create_node(struct arena *arena, enum parser_node_type type, int line){
    struct parser_node *node = arena_alloc(arena, sizeof(struct parser_node));
    node->type = type;
    node->line = line;
    node->is_literal_data_created_by_parser = 0;
    node->type_info = NULL;
    node->right_node = NULL;
    node->left_node = NULL;

    return node;
}

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node){
    if(NULL == node) {LOG_M_ERR("parser_parser_add_node - \"struct parser_node *node\" is null"); return;}
    if(NULL == parser) {LOG_M_ERR("parser_parser_add_node - \"struct parser_t *parser\" is null"); return;}

    vector_add(parser->nodes, &node);
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

#define EAT_OR_RETURN(parser, tokens, count, cursor, type) \
    do { \
        if (!eat(tokens, count, cursor, type)) { \
            (parser)->successful = 0; \
            return NULL; \
        } \
    } while(0) 

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

    struct parser_node *module = parser_create_node(parser->arena, PARSER_NODE_MODULE, 0);
    module->data.module.functions = vector_create_vector(parser->arena, 4, sizeof(struct parser_node *));
    module->data.module.name = lexer_extract_module_name(parser->arena, file->file_name);
    parser_parser_add_node(parser, module);

    while (cursor < file->token_count) {
        struct parser_node *node = parser_parse_statement(parser, file->tokens, file->token_count, &cursor);
        if (node) {
            vector_add(module->data.module.functions, &node);
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
    struct str_view var_name = tokens[*cursor].str_view;

    EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

    EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON);
    
    int pointer_level = calculate_pointer_level(tokens, cursor);

    struct lexer_token *type_name_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER); 
    if(NULL == type_name_token) {parser->successful = 0; return NULL;}

    struct str_view type_name = type_name_token->str_view;

    struct type_info *type_info = type_table_get_or_create_pointer_type_info(parser->type_table, type_name, pointer_level);

    if(NULL == type_info) {
        C_LOG_ERR("unknown type (" SV_FMT "), on line %d", SV_ARG(type_name_token->str_view), tokens[*cursor].line);
        LOG_M_ERR("parser_parse_variable_declaration - \"struct type_info *type_info\" is null");
        parser->successful = 0;
        return NULL;
    }

    EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL);

    struct parser_node *value_node = parser_parse_bitwise_or(parser, tokens, token_count, cursor);
    if(NULL == value_node) {parser->successful = 0; return NULL;}
    struct parser_node *decl_node = parser_create_node(parser->arena, PARSER_NODE_VARIABLE_DECLARATION, name_token.line);
    if(NULL == decl_node) {parser->successful = 0; return NULL;}

    decl_node->data.variable.variable_name = var_name;
    if(str_view_is_empty(decl_node->data.variable.variable_name)) {
        parser->successful = 0;
        return NULL;
    }
    
    decl_node->right_node = value_node; 

    bool is_global = parser->current_scope->is_global_table;
    if(str_view_eq_cstr(type_info->name, "string")) is_global = true;

    if(NULL == symbol_table_define(parser->current_scope, var_name, type_info, SYMBOL_KIND_VARIABLE, pointer_level, is_global)) {
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
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_RESILIENT){
        return parser_parse_resilient_block(parser, tokens, token_count, cursor, 1);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_FN){
        return parser_parse_function(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_LOOP){
        return parser_parse_loop(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_ASM){
        return parser_parse_asm(parser, tokens, token_count, cursor);
    }

    struct parser_node *node = NULL;
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_VAR) {
        node = parser_parse_variable_declaration(parser, tokens, token_count, cursor);
    }else {
        node = parser_parse_bitwise_or(parser, tokens, token_count, cursor);
    }

    if (NULL == node) {parser->successful = 0; return NULL;}

    if (node->type != PARSER_NODE_BLOCK && node->type != PARSER_NODE_FUNCTION && node->type != PARSER_NODE_LOOP) {
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_SEMICOLON);
    }

    return node;
}

struct parser_node *parser_parse_assignment(struct parser_t *parser, struct lexer_token *tokens, int token_count, int *cursor) {
    int line = tokens[*cursor].line;

    struct parser_node *left_node = parser_parse_bitwise_or(parser, tokens, token_count, cursor);
    if(NULL == left_node) {
        parser->successful = 0;
        return NULL;
    }

    EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL);

    struct parser_node *right_node = parser_parse_bitwise_or(parser, tokens, token_count, cursor);
    if(NULL == right_node) {
        parser->successful = 0;
        return NULL;
    }

    struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_VARIABLE_ASSIGMENT, line);
    node->left_node = left_node;
    node->right_node = right_node;
    
    node->type_info = left_node->type_info;

    return node;
}

struct parser_node *parser_parse_call(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, struct str_view func_name) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_call - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *call_node = parser_create_node(parser->arena, PARSER_NODE_CALL, tokens[*cursor].line);
    if(NULL == call_node){
        LOG_M_ERR("parser_parse_call - \"struct parser_node *call_node\" is null");
        parser->successful = 0;
        return NULL;
    }

    EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

    call_node->data.call.name = func_name;
    call_node->data.call.args = vector_create_vector(parser->arena, 4, sizeof(struct parser_node *));
    call_node->data.call.arg_count = 0;

    if (!call_node->data.call.args) {
        parser->successful = 0;
        return NULL;
    }

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RPAREN) {

        struct parser_node *arg = parser_parse_bitwise_and(parser, tokens, token_count, cursor);
        if(NULL == arg){
            parser->successful = 0;
            return NULL;
        }

        vector_add(call_node->data.call.args, &arg);

        if (*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_COMMA) {
            EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_COMMA);
        }
    }
    call_node->data.call.arg_count = call_node->data.call.args->element_count;

    if(*cursor >= token_count || NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {
        parser->successful = 0;
        return NULL;
    }

    return call_node;
}

struct parser_node *parser_parse_parameters(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_parameters - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *params_node = parser_create_node(parser->arena, PARSER_NODE_BLOCK, tokens[*cursor].line);
    if(NULL == params_node){
        LOG_M_ERR("parser_parse_parameters - \"struct parser_node *params_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    params_node->data.block.statements = vector_create_vector(parser->arena, 4, sizeof(struct parser_node *));
    params_node->data.block.count = 0;
    ++parser->block_counter;
    params_node->data.block.id = parser->block_counter;

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RPAREN) {
        struct parser_node *p_node = parser_create_node(parser->arena, PARSER_NODE_VARIABLE_DECLARATION, tokens[*cursor].line);
        p_node->left_node = NULL;
        p_node->right_node = NULL;

        p_node->data.variable.variable_name = tokens[*cursor].str_view;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);


        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON);

        int pointer_level = calculate_pointer_level(tokens, cursor);
        p_node->type_info = type_table_get_or_create_pointer_type_info(parser->type_table, tokens[*cursor].str_view, pointer_level);
        if(NULL == p_node->type_info) {
            C_LOG_ERR("Unknown parameter type on line: %d", tokens[*cursor].line);
            parser->successful = 0;
            return NULL;
        }
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        vector_add(params_node->data.block.statements, &p_node);

        if (*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_COMMA) {
            EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_COMMA);

            if (tokens[*cursor].type == LEXER_TOKEN_TYPE_RPAREN) {
                C_LOG_ERR("Trailing comma in parameters is not allowed!");
                parser->successful = 0;
                return NULL;
            }
        }

    }
    params_node->data.block.count = params_node->data.block.statements->element_count;

    params_node->data.block.mangled_name = parser_block_generate_mangled_name(parser, params_node);
    return params_node;
}

struct parser_node *parser_parse_loop(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_loop - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *loop_node = parser_create_node(parser->arena, PARSER_NODE_LOOP, tokens[*cursor].line);
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

    int approx_value = 0;

    if(LEXER_TOKEN_TYPE_APPROX == tokens[*cursor].type) {
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_APPROX);
        approx_value = str_view_to_int(tokens[*cursor].str_view);
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_INT_LITERAL);
    }

    struct parser_node *loop_body = parser_parse_block(parser, tokens, token_count, cursor, 1);
    if(NULL == loop_body) goto cleanup_err_level_0;
    loop_node->data.loop.body_block = loop_body;
    loop_body->data.loop.approx_value = approx_value;


    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RETURN)) goto cleanup_err_level_0;
    parser->current_scope = loop_body->data.block.scope;

    struct parser_node *return_block = parser_parse_block(parser, tokens, token_count, cursor, 0);
    if(NULL == return_block) goto cleanup_err_level_0;
    loop_node->data.loop.return_block = return_block;

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_CONTINUE)) goto cleanup_err_level_0;
    parser->current_scope = loop_body->data.block.scope;

    struct parser_node *continue_block = parser_parse_block(parser, tokens, token_count, cursor, 0);
    if(NULL == continue_block) goto cleanup_err_level_0;
    loop_node->data.loop.continue_block = continue_block;

    parser->current_scope = loop_body->data.block.scope->parent;
    parser->loop_depth_counter--;
    parser->current_loop_id = old_current_loop_id;
    return loop_node;

cleanup_err_level_0:
    parser->successful = 0;
    return NULL;
}


struct parser_node *parser_parse_asm(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){

    if(NULL == parser) {LOG_M_ERR("parser_parse_asm - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *asm_node = parser_create_node(parser->arena, PARSER_NODE_ASM, tokens[*cursor].line);
    if(NULL == asm_node){
        LOG_M_ERR("parser_parse_asm - \"struct parser_node *continue_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_ASM)) goto cleanup_err_level_0;

    struct lexer_token *assembly_data = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_STRING_LITERAL);
    if (NULL == assembly_data) {
        C_LOG_ERR("String Literal (assembly) expected, line %d", asm_node->line); 
        goto cleanup_err_level_0;
    }

    struct str_view asm_view = str_view_trim_left(assembly_data->str_view);

    asm_node->data.assembly.assembly_data = asm_view;

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_SEMICOLON)) {C_LOG_ERR("expected \";\" on line %d", asm_node->line); goto cleanup_err_level_0;}

    return asm_node;
cleanup_err_level_0:
    parser->successful = 0;
    return NULL;
}

struct parser_node *parser_parse_function(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_function - \"struct parser_t *restrict parser\" is null"); return NULL;}
    if(parser->current_scope && parser->current_scope->parent) {
        C_LOG_ERR("cannot define a function in another function, line: %d", tokens[*cursor].line);
        parser->successful = 0;
        return NULL;
    }
    struct parser_node *function_node = parser_create_node(parser->arena, PARSER_NODE_FUNCTION, tokens[*cursor].line);
    if(NULL == function_node){
        LOG_M_ERR("parser_parse_function - \"struct parser_node *function_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    function_node->data.function.flags = bitset_create(parser->arena, 8);

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_FN)) {parser->successful = 0;return NULL;}
    struct str_view name = tokens[*cursor].str_view; 
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser->successful = 0;return NULL;}
    
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN)) {parser->successful = 0;return NULL;}
    struct parser_node *parameters = parser_parse_parameters(parser, tokens, token_count, cursor);
    if(NULL == parameters || NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)) {parser->successful = 0;return NULL;}

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON)) {parser->successful = 0;return NULL;}
    struct str_view return_type_name = tokens[*cursor].str_view;

    int pointer_level = calculate_pointer_level(tokens, cursor);
    struct type_info *ret_type = type_table_get_type_info(parser->type_table, return_type_name, pointer_level);
    if(NULL == ret_type) {C_LOG_ERR("Unknown return type for function on line: %d", tokens[*cursor].line);parser->successful = 0; return NULL;}
    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)) {parser->successful = 0;return NULL;}

    if(LEXER_TOKEN_TYPE_PURE == tokens[*cursor].type) {
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_PURE);
        bitset_set(function_node->data.function.flags, FUNC_FLAG_IS_PURE);
    }

    struct symbol_table *body_scope = symbol_table_create_symbol_table(parser->symbol_arena, parser->current_scope, &parser->scope_counter);
    if(NULL == body_scope){
        parser->successful = 0;
        return NULL;
    }
    
    for(int i = 0; i < parameters->data.block.count; i++) {
        struct parser_node *p = *(struct parser_node **) vector_get(parameters->data.block.statements, i);
        struct symbol_t *sym = symbol_table_define(body_scope, p->data.variable.variable_name, p->type_info, SYMBOL_KIND_VARIABLE, p->type_info->pointer_level, body_scope->is_global_table);
        if(NULL == sym) {parser->successful = 0;return NULL;}
        p->data.variable.symbol = sym;
    }

    struct symbol_table *old_scope = parser->current_scope;
    parser->current_scope = body_scope;

    function_node->data.function.body = parser_parse_block(parser, tokens, token_count, cursor, 0);
    if(NULL == function_node->data.function.body) {
        parser->current_scope = old_scope;
        parser->successful = 0;
        return NULL;
    }
    function_node->data.function.body->data.block.owns_scope = 1;
    
    parser->current_scope = old_scope;

    function_node->data.function.params = parameters;
    function_node->data.function.return_type = ret_type;
    function_node->data.function.param_count = parameters->data.block.count;
    function_node->data.function.name = name;
    function_node->data.function.mangled_name = parser_function_generate_mangled_name(parser, function_node);

    struct symbol_t *sym = symbol_table_define(parser->current_scope, function_node->data.function.name, type_table_get_type_info_cstr(parser->type_table, "fn", 0), SYMBOL_KIND_FUNCTION, 0, parser->current_scope->is_global_table);
    if(NULL == sym) {
        parser->successful = 0;
        return NULL;
    }else {
        sym->flags = function_node->data.function.flags;
        sym->function.parameters = function_node->data.function.params;
        sym->function.return_type = function_node->data.function.return_type;
    }
    return function_node;
}

struct parser_node *parser_parse_resilient_block(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int create_new_scope) {
    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RESILIENT);
    struct parser_node *block = parser_parse_block(parser, tokens, token_count, cursor, create_new_scope);
    block->data.block.is_resilient = 1;
    return block;
}

struct parser_node *parser_parse_block(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int create_new_scope) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_block - \"struct parser_t *restrict parser\" is null"); return NULL;}
    if (*cursor >= token_count) {parser->successful = 0; return NULL;}
    int line_number = tokens[*cursor].line;
    struct parser_node *block_node = parser_create_node(parser->arena, PARSER_NODE_BLOCK, line_number);
    ++parser->block_counter;
    block_node->data.block.id = parser->block_counter;
    block_node->data.block.is_resilient = 0;
    if(NULL == block_node){
        LOG_M_ERR("parser_parse_block - \"struct parser_node *block_node\" is null");
        parser->successful = 0;
        return NULL;
    }
    
    if(1 == create_new_scope) {
        parser->current_scope = symbol_table_create_symbol_table(parser->symbol_arena, parser->current_scope, &parser->scope_counter);
    }
    block_node->data.block.owns_scope = create_new_scope;
    block_node->data.block.scope = parser->current_scope;
    block_node->data.block.mangled_name = parser_block_generate_mangled_name(parser, block_node);

    if(NULL == eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LBRACE)) {
        if(1 == create_new_scope) parser->current_scope = parser->current_scope->parent;
        parser->successful = 0;
        return NULL;
    }

    block_node->data.block.statements = vector_create_vector(parser->arena, 16, sizeof(struct parser_node*)); 
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

        vector_add(block_node->data.block.statements, &stmt);
    }
    block_node->data.block.count = block_node->data.block.statements->element_count;

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

struct parser_node *parser_parse_bitwise_or(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_bitwise_or - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_bitwise_xor(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_EQUAL) {
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL); 
        struct parser_node *right = parser_parse_bitwise_or(parser, tokens, token_count, cursor);

        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_VARIABLE_ASSIGMENT, tokens[*cursor].line);
        node->left_node = left;
        node->right_node = right;
        node->type_info = left->type_info;
        return node;
    }

    while(*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_OR) {
        int op_line = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_OR);

        struct parser_node *right = parser_parse_bitwise_xor(parser, tokens, token_count, cursor);
        if(!right) { parser->successful = 0; return NULL; }

        struct parser_node *new_node = parser_create_node(parser->arena, PARSER_NODE_BITWISE_OR, op_line);
        new_node->left_node = left;
        new_node->right_node = right;
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_bitwise_xor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_bitwise_xor - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_bitwise_and(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }
    while(*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_XOR) {
        int op_line = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_XOR);

        struct parser_node *right = parser_parse_bitwise_and(parser, tokens, token_count, cursor);
        if(!right) { parser->successful = 0; return NULL; }

        struct parser_node *new_node = parser_create_node(parser->arena, PARSER_NODE_BITWISE_XOR, op_line);
        new_node->left_node = left;
        new_node->right_node = right;
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_bitwise_and(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_bitwise_and - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }
    while(*cursor < token_count && tokens[*cursor].type == LEXER_TOKEN_TYPE_AMPERSAND) {
        int op_line = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_AMPERSAND);

        struct parser_node *right = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        if(!right) { parser->successful = 0; return NULL; }

        struct parser_node *new_node = parser_create_node(parser->arena, PARSER_NODE_BITWISE_AND, op_line);
        new_node->left_node = left;
        new_node->right_node = right;
        left = new_node;
    }
    return left;
}

struct parser_node *parser_parse_boolean_logic(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_shift_operators(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }

    int node_type_id = is_boolean_logic_token(tokens[*cursor].type);
    while(*cursor < token_count && node_type_id != -1) {
        int op_line = tokens[*cursor].line;
        enum parser_node_type op_type = node_type_id;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_shift_operators(parser, tokens, token_count, cursor);
        if(NULL == right){
            C_LOG_ERR("Expected expression after boolean operator on line %d", op_line);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(parser->arena, op_type, op_line);
        if(NULL == new_node) {
            parser->successful = 0;
            return NULL;
        }
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

        if (*cursor < token_count) {
            node_type_id = is_boolean_logic_token(tokens[*cursor].type);
        } else {
            node_type_id = -1;
        }
    }
    return left;
}

struct parser_node *parser_parse_shift_operators(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_shift_operators - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_expression(parser, tokens, token_count, cursor);
    if(NULL == left){
        parser->successful = 0;
        return NULL;
    }
    int node_type_id = is_shift_operator_token(tokens[*cursor].type);
    while(*cursor < token_count && node_type_id != -1) {
        int op_line = tokens[*cursor].line;
        enum parser_node_type op_type = node_type_id;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_expression(parser, tokens, token_count, cursor);
        if(NULL == right){
            C_LOG_ERR("Expected expression after shift operator on line %d", op_line);
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(parser->arena, op_type, op_line);
        if(NULL == new_node) {
            parser->successful = 0;
            return NULL;
        }
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

        if (*cursor < token_count) {
            node_type_id = is_shift_operator_token(tokens[*cursor].type);
        } else {
            node_type_id = -1;
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
        EAT_OR_RETURN(parser, tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_term(parser, tokens, token_count, cursor);
        if(!right){
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(parser->arena, op_type, op_line);
        if(NULL == new_node) {
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

    while(*cursor < token_count && (LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type || LEXER_TOKEN_TYPE_SLASH == tokens[*cursor].type) || LEXER_TOKEN_TYPE_PERCENT == tokens[*cursor].type) {
        enum parser_node_type op_type = PARSER_NODE_DIVIDE;
        if(LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type)         op_type = PARSER_NODE_MUL;
        else if(LEXER_TOKEN_TYPE_PERCENT == tokens[*cursor].type) op_type = PARSER_NODE_MOD;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_unary(parser, tokens, token_count, cursor);
        if(NULL == right){
            parser->successful = 0;
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(parser->arena, op_type, tokens[*cursor].line);
        if(NULL == new_node) {
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

        if(NULL == t || str_view_is_empty(t->str_view)) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.literal_data = t->str_view;
        node->type_info = NULL;
        return node;
    }else if(LEXER_TOKEN_TYPE_STRING_LITERAL == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_STRING_LITERAL);

        if(NULL == t || str_view_is_empty(t->str_view)) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_STRING, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.literal_data = t->str_view;
        node->type_info = NULL;
        return node;
    }else if(LEXER_TOKEN_TYPE_CHAR == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_CHAR);

        if(NULL == t || str_view_is_empty(t->str_view)) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_CHAR, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.literal_data = t->str_view;
        node->type_info = NULL;
        return node;
    }else if(LEXER_TOKEN_TYPE_IDENTIFIER == tokens[*cursor].type){
        if((*cursor)+1 < token_count && tokens[(*cursor)+1].type == LEXER_TOKEN_TYPE_LPAREN) {
            struct str_view name_to_pass = tokens[*cursor].str_view;
            EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
            return parser_parse_call(parser, tokens, token_count, cursor, name_to_pass);
        }
        int line_number = tokens[*cursor].line;
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        if(NULL == t || str_view_is_empty(t->str_view)) {parser->successful = 0; return NULL;}
        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_IDENTIFIER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->data.variable.variable_name = t->str_view;

        struct symbol_t *sym = symbol_table_look_up(parser->current_scope, node->data.variable.variable_name);
        if(NULL == sym || NULL == sym->type){
            C_LOG_ERR("undefined variable (" SV_FMT "), line: %d", SV_ARG(node->data.variable.variable_name), node->line);
            parser->successful = 0;
            return NULL;
        }
        node->type_info = sym->type;
        return node;
    }else if(LEXER_TOKEN_TYPE_LPAREN == tokens[*cursor].type){
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

        struct parser_node *node = parser_parse_bitwise_or(parser, tokens, token_count, cursor);
        if(NULL == node) {parser->successful = 0; return NULL;}
        if(*cursor >= token_count) {parser->successful = 0; return NULL;}
        int line_number = tokens[*cursor].line;

        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN);
        return node;
    }else if(LEXER_TOKEN_TYPE_SIZEOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_SIZEOF);
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

        int pointer_level = calculate_pointer_level(tokens, cursor);
        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        struct type_info *type_info = NULL;
        if(NULL == type_token) {
            (*cursor)--;
            struct parser_node *right = parser_parse_factor(parser, tokens, token_count, cursor);
            type_info = type_table_get_type_info(parser->type_table, right->data.literal_data, pointer_level);
        }else {
            type_info = type_table_get_type_info(parser->type_table, type_token->str_view, pointer_level);
        }

        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"sizeof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN);

        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", type_info->size);
        node->data.literal_data = str_view_make(buf, sizeof(buf));
        return node;
    }else if(LEXER_TOKEN_TYPE_ALIGNOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_ALIGNOF);
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);
        int pointer_level = calculate_pointer_level(tokens, cursor);

        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        struct type_info *type_info = NULL;
        if(NULL == type_token) {
            (*cursor)--;
            struct parser_node *right = parser_parse_factor(parser, tokens, token_count, cursor);
            type_info = type_table_get_type_info(parser->type_table, right->data.literal_data, pointer_level);
        }else {
            type_info = type_table_get_type_info(parser->type_table, type_token->str_view, pointer_level);
        }

        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"sizeof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN);

        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", type_table_size_padding(type_info->size));
        node->data.literal_data = str_view_make(buf, sizeof(buf));
        return node;
    }else if(LEXER_TOKEN_TYPE_TYPEOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_TYPEOF);
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);
        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        if(NULL == type_token) {
            C_LOG_ERR("expected an identifier for \"typeof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct symbol_t *sym = symbol_table_look_up(parser->current_scope, type_token->str_view);
        struct type_info *type_info = sym->type;
        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"typeof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN);

        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_STRING, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        node->data.literal_data = type_info->name;
        return node;
    }else if(LEXER_TOKEN_TYPE_STOF == tokens[*cursor].type){
        int line_number = tokens[*cursor].line;
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_STOF);
        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);
        struct lexer_token *type_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
        if(NULL == type_token) {
            C_LOG_ERR("expected an identifier for \"stof\"on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }
        struct symbol_t *sym = symbol_table_look_up(parser->current_scope, type_token->str_view);
        struct type_info *type_info = sym->type;
        if(NULL == type_info) {
            C_LOG_ERR("expected a valid type after \"(\" for \"stof\" on line: %d", line_number);
            parser->successful = 0;
            return NULL;
        }

        EAT_OR_RETURN(parser, tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN);

        struct parser_node *node = parser_create_node(parser->arena, PARSER_NODE_NUMBER, line_number);
        if(NULL == node) {parser->successful = 0; return NULL;}
        node->is_literal_data_created_by_parser = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", type_info->size);
        node->data.literal_data = str_view_make(buf, sizeof(buf));
        return node;
    }else if(LEXER_TOKEN_TYPE_LOOP == tokens[*cursor].type){
        struct parser_node *node = parser_parse_loop(parser, tokens, token_count, cursor);
        return node;
    }else {
        C_LOG_ERR("parser_parse_factor - current token (" SV_FMT ") is not literal or identifier (unexpected token), line: %d", SV_ARG(tokens[*cursor].str_view) ,tokens[*cursor].line);
        (*cursor)++;
        parser->successful = 0;
        return NULL;
    }
    return NULL;
}

struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(*cursor >= token_count) {LOG_M_ERR("parser_parse_unary - \"*cursor >= token_count\""); parser->successful = 0;return NULL;}
    enum token_type tok_type = tokens[*cursor].type;
    if (LEXER_TOKEN_TYPE_MINUS == tok_type || LEXER_TOKEN_TYPE_PLUS == tok_type || LEXER_TOKEN_TYPE_BANG == tok_type || LEXER_TOKEN_TYPE_STAR == tok_type || LEXER_TOKEN_TYPE_AMPERSAND == tok_type || LEXER_TOKEN_TYPE_NOT == tok_type) {
        int op_line = tokens[*cursor].line;
        struct lexer_token *op_token = NULL;
        enum parser_node_type parser_node_type;

        if(LEXER_TOKEN_TYPE_PLUS == tok_type) {parser_node_type = PARSER_NODE_UNDEFINED; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_PLUS);}
        else if(LEXER_TOKEN_TYPE_MINUS == tok_type) {parser_node_type = PARSER_NODE_UNARY_MINUS; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_MINUS);}
        else if(LEXER_TOKEN_TYPE_BANG == tok_type) {parser_node_type = PARSER_NODE_UNARY_BANG; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_BANG);}
        else if(LEXER_TOKEN_TYPE_STAR == tok_type) {parser_node_type = PARSER_NODE_UNARY_DEREFERENCE; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_STAR);}
        else if(LEXER_TOKEN_TYPE_AMPERSAND == tok_type) {parser_node_type = PARSER_NODE_UNARY_ADDRESS_OF; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_AMPERSAND);}
        else if(LEXER_TOKEN_TYPE_NOT == tok_type) {parser_node_type = PARSER_NODE_UNARY_NOT; op_token = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_NOT);}
        if(NULL == op_token) {parser->successful = 0; return NULL;}

        struct parser_node *right_node = parser_parse_unary(parser, tokens, token_count, cursor);
        if(NULL == right_node) {parser->successful = 0; return NULL;}
        

        if (LEXER_TOKEN_TYPE_PLUS == op_token->type) {
            return right_node; 
        }

        struct parser_node *node = parser_create_node(parser->arena, parser_node_type, op_line);
        if(NULL == node) {
            parser->successful = 0;
            return NULL;
        }

        node->right_node = right_node;
        
        return node;
    }

    return parser_parse_factor(parser, tokens, token_count, cursor);
}
