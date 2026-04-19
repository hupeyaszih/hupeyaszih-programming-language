#include "parser.h"
#include "globals.h"
#include "lexer.h"
#include <stdio.h>
#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>


struct eval_result parser_eval(struct parser_node *node, struct type_table *restrict type_table, struct symbol_table *restrict current_scope) { // For testing
    struct eval_result res = {NULL, NULL};
    if (NULL == node) return res;
    if (PARSER_NODE_NUMBER == node->type) {
        res.type = type_table_get_type_info(type_table, "int32");

        res.raw_data = malloc(res.type->size); 

        if (res.raw_data) {
            int value = atoi(node->data.literal_data);
            memcpy(res.raw_data, &value, res.type->size);
        }

        return res;
    }

    if (PARSER_NODE_IDENTIFIER == node->type) {

        struct symbol_t *sym = symbol_table_look_up(current_scope, node->data.variable_name);

        if (sym) {
            if (NULL != sym->value) {
                res.type = sym->type;
                res.raw_data = malloc(sym->type->size);
                if (res.raw_data) {
                    memcpy(res.raw_data, sym->value, sym->type->size);
                }
            } else {
                C_LOG_ERR("Variable '%s' is null", node->data.variable_name);
            }
        } else {
            C_LOG_ERR("Variable '%s' is undefined!", node->data.variable_name);
        }

        return res; 
    }

    if (PARSER_NODE_VARIABLE_ASSIGMENT == node->type) {
        struct eval_result right_res = parser_eval(node->right_node, type_table, current_scope);
        struct symbol_t *sym = symbol_table_look_up(current_scope, node->data.variable_name);

        if (sym && right_res.raw_data) {
            symbol_table_assign(sym, right_res.raw_data); 
        }
        return right_res; 
    }


    if (PARSER_NODE_BLOCK == node->type) {
        struct eval_result block_res = {NULL, NULL};
        for (int i = 0; i < node->data.block.count; i++) {
            if(block_res.raw_data) free(block_res.raw_data); 
            block_res = parser_eval(node->data.block.statements[i], type_table, node->data.block.scope);
        }
        return block_res;
    }


    if (PARSER_NODE_VARIABLE_DECLARATION == node->type) {
        struct eval_result right_res = parser_eval(node->right_node, type_table, current_scope);

        if (NULL != right_res.raw_data) {
            struct symbol_t *sym = symbol_table_look_up(current_scope, node->data.variable_name);

            if (sym) {
                symbol_table_assign(sym, right_res.raw_data);
            }else {
                LOG_M_ERR("Couldn't get the symbol: '%s'", node->data.variable_name);
            }
        }
        return right_res; 
    }

    if (PARSER_NODE_UNARY_BANG == node->type) {
        res = parser_eval(node->right_node, type_table, current_scope);

        if (res.raw_data && res.type) {
            if (0 == strcmp(res.type->name, "int32")) {
                *(int*)res.raw_data = !(*(int*)res.raw_data);
            } 
        }
        return res;
    }else if (PARSER_NODE_UNARY_MINUS == node->type) {
        res = parser_eval(node->right_node, type_table, current_scope);

        if (res.raw_data && res.type) {
            if (0 == strcmp(res.type->name, "int32")) {
                *(int*)res.raw_data = -(*(int*)res.raw_data);
            } else if (0 == strcmp(res.type->name, "float64")) {
                *(double*)res.raw_data = -(*(double*)res.raw_data);
            }
        }
        return res;
    }

    struct eval_result left = parser_eval(node->left_node, type_table, current_scope);
    struct eval_result right = parser_eval(node->right_node, type_table, current_scope);

    if (left.type != right.type) {
        C_LOG_ERR("Types don't match: %s and %s!", left.type->name, right.type->name);
        goto exit_parser_eval;
    }

    res.raw_data = malloc(left.type->size);
    res.type = left.type;

    switch (node->type) {
        case PARSER_NODE_PLUS:   
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data + *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(double*)res.raw_data = *(double*)left.raw_data + *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_MINUS:  
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data - *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(double*)res.raw_data = *(double*)left.raw_data - *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_MUL:
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data * *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(double*)res.raw_data = *(double*)left.raw_data * *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_DIVIDE: 
            if (0 == strcmp(left.type->name, "int32")) {
                if(0 == (*(int*)right.raw_data)){
                    C_LOG_ERR("cannot divide 0 by 0!");
                    free(res.raw_data);
                    goto exit_parser_eval;
                }
                *(int*)res.raw_data = *(int*)left.raw_data / *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                if(0 == (*(double*)right.raw_data)){
                    C_LOG_ERR("cannot divide 0.0 by 0.0!");
                    free(res.raw_data);
                    goto exit_parser_eval;
                }
                *(double*)res.raw_data = *(double*)left.raw_data / *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_EQUAL_EQUAL: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data == *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data == *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_BANG_EQUAL: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data != *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data != *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_GREATER_EQUAL: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data >= *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data >= *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_LESS_EQUAL: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data <= *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data <= *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_GREATER: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data > *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data > *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        case PARSER_NODE_LESS: 
            if (0 == strcmp(left.type->name, "int32")) {
                *(int*)res.raw_data = *(int*)left.raw_data < *(int*)right.raw_data;
            } else if (0 == strcmp(left.type->name, "float64")) {
                *(int*)res.raw_data = *(double*)left.raw_data < *(double*)right.raw_data;
            }
            goto exit_parser_eval;
        default:
            C_LOG_ERR("parser_eval - UNDEFINED NODE TYPE");
            free(res.raw_data);
            goto exit_parser_eval;
    }

exit_parser_eval:
    free(left.raw_data);
    free(right.raw_data);
    return res;

}

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
    struct parser_t *parser = malloc(sizeof(struct parser_t));
    if(!parser) {C_LOG_ERR("parser_create_parser - couldn't create parser"); free(parser); return NULL;}
    parser->node_count = 0;
    parser->nodes = NULL;
    parser->current_scope = NULL;

    return parser;
}
void parser_delete_parser(struct parser_t **parser){
    if(NULL == parser || NULL == *parser) {C_LOG_ERR("parser_delete_parser - \"struct parser_t **parser\" or \"*parser\" is null"); return;}
    free((*parser)->nodes);
    free((*parser));
    *parser = NULL;
}

struct parser_node *parser_create_node(enum parser_node_type type, int line){
    struct parser_node *node = malloc(sizeof(struct parser_node));
    node->type = type;
    node->line = line;

    return node;
}
void parser_delete_node(struct parser_node **node){
    if(NULL == node || NULL == (*node)) {LOG_M_ERR("parser_delete_node - \"struct parser_node *node\" is null"); return;}
    if((*node)->left_node) parser_delete_node(&((*node)->left_node));
    if((*node)->right_node) parser_delete_node(&((*node)->right_node));
    if ((*node)->type == PARSER_NODE_BLOCK) {
        if ((*node)->data.block.scope) {
            symbol_table_delete_symbol_table(&((*node)->data.block.scope));
        }
    }

    free((*node));
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
        else {C_LOG_ERR("unexpected token type (\"%s\") on line %d", lexer_token_type_to_string(tokens[*cursor].type),tokens[*cursor].line);}
        return NULL;
    }
    return &tokens[(*cursor)++];
}


int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file) {
    int cursor = 0;
    while (cursor < file->token_count) {
        struct parser_node *node = parser_parse_statement(parser, file->tokens, file->token_count, &cursor);
        if (node) {
            parser_parser_add_node(parser, node);
        } else {
            return 0; 
        }
    }
    return 1;
}

struct parser_node *parser_parse_variable_declaration(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(NULL == parser) {LOG_M_ERR("parser_parse_variable_declaration - \"struct parser_t *restrict parser\" is null"); return NULL;}

    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_VAR);

    if(tokens[*cursor].type != LEXER_TOKEN_TYPE_IDENTIFIER) {
        LOG_M_ERR("Expected identifier after 'var', line: %d", tokens[*cursor].line);
        return NULL;
    }
    struct lexer_token name_token = tokens[*cursor];
    char *var_name = tokens[*cursor].token;

    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

    char *type_name = NULL;
    if(tokens[*cursor].type == LEXER_TOKEN_TYPE_COLON) {
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_COLON);
        type_name = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER)->token; // int32, float32 vb.
    }

    struct type_info *type_info = type_table_get_type_info(parser->type_table, type_name);
    if(NULL == type_info) {
        LOG_M_ERR("parser_parse_variable_declaration - \"struct type_info *type_info\" is null");
        return NULL;
    }

    symbol_table_define(parser->current_scope, var_name, type_info);
    if(tokens[*cursor].type != LEXER_TOKEN_TYPE_EQUAL) {
        LOG_M_ERR("Expected '=' in variable declaration, line: %d", tokens[*cursor].line);
        return NULL;
    }
    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL);

    struct parser_node *value_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    struct parser_node *decl_node = parser_create_node(PARSER_NODE_VARIABLE_DECLARATION, name_token.line);
    decl_node->data.variable_name = var_name;
    
    decl_node->right_node = value_node; 

    return decl_node;
}

struct parser_node *parser_parse_statement(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_LBRACE) {
        return parser_parse_block(parser, tokens, token_count, cursor);
    }

    struct parser_node *node = NULL;
    if (tokens[*cursor].type == LEXER_TOKEN_TYPE_VAR) {
        node = parser_parse_variable_declaration(parser, tokens, token_count, cursor);
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_IDENTIFIER) {
        if ((*cursor + 1 < token_count) && tokens[*cursor + 1].type == LEXER_TOKEN_TYPE_EQUAL) {
            node = parser_parse_assignment(parser, tokens, token_count, cursor);
        } else {
            node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        }
    }else {
        node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    }


    if (node && PARSER_NODE_BLOCK != node->type) {
        if (!eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_SEMICOLON)) {
            C_LOG_ERR("expected ';' on line %d.", tokens[*cursor].line);
        }
    }

    return node;
}

struct parser_node *parser_parse_assignment(struct parser_t *parser, struct lexer_token *tokens, int token_count, int *cursor) {
    int line = tokens[*cursor].line;
    char *var_name = tokens[*cursor].token;
    
    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);
    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_EQUAL);
    
    struct parser_node *value_node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
    
    struct parser_node *node = parser_create_node(PARSER_NODE_VARIABLE_ASSIGMENT, line);
    node->data.variable_name = var_name;
    node->right_node = value_node;
    
    return node;
}

struct parser_node *parser_parse_block(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    struct parser_node *block_node = parser_create_node(PARSER_NODE_BLOCK, tokens[*cursor].line);
    
    parser->current_scope = symbol_table_create_symbol_table(parser->current_scope);
    block_node->data.block.scope = parser->current_scope;

    eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LBRACE);

    int capacity = 16;
    block_node->data.block.statements = malloc(sizeof(struct parser_node*) * capacity); 
    block_node->data.block.count = 0;

    while (*cursor < token_count && tokens[*cursor].type != LEXER_TOKEN_TYPE_RBRACE) {
        struct parser_node *stmt = parser_parse_statement(parser, tokens, token_count, cursor);
        if (stmt) {
            if (block_node->data.block.count >= capacity) {
                printf("yep\n");
                capacity *= 2;
                block_node->data.block.statements = realloc(block_node->data.block.statements, sizeof(struct parser_node*) * capacity);
            }
            block_node->data.block.statements[block_node->data.block.count++] = stmt;
        }
    }

    if (!eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RBRACE)) {
        C_LOG_ERR("expected '}' on line: %d", tokens[*cursor].line);
    }

    parser->current_scope = parser->current_scope->parent;

    return block_node;
}

struct parser_node *parser_parse_boolean_logic(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(NULL == parser) {LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_t *restrict parser\" is null"); return NULL;}
    struct parser_node *left = parser_parse_expression(parser, tokens, token_count, cursor);
    if(NULL == left){
        LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_node *left\" is null");
        return NULL;
    }

    int node_type_id = is_boolean_logic_token(tokens[*cursor].type);
    while(*cursor < token_count && node_type_id != 0) {
        enum parser_node_type op_type = node_type_id;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_expression(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_node *right\" is null");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
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
        LOG_M_ERR("parser_parse_expression - \"struct parser_node *left\" is null");
        return NULL;
    }

    while(*cursor < token_count && (LEXER_TOKEN_TYPE_PLUS == tokens[*cursor].type || LEXER_TOKEN_TYPE_MINUS == tokens[*cursor].type)) {
        enum parser_node_type op_type = PARSER_NODE_MINUS;
        if(LEXER_TOKEN_TYPE_PLUS == tokens[*cursor].type) op_type = PARSER_NODE_PLUS;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_term(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_expression - \"struct parser_node *right\" is null");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
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
        LOG_M_ERR("parser_parse_term - \"struct parser_node *left\" is null");
        return NULL;
    }

    while(*cursor < token_count && (LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type || LEXER_TOKEN_TYPE_SLASH == tokens[*cursor].type)) {
        enum parser_node_type op_type = PARSER_NODE_DIVIDE;
        if(LEXER_TOKEN_TYPE_STAR == tokens[*cursor].type) op_type = PARSER_NODE_MUL;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_unary(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_term - \"struct parser_node *right\" is null");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
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
        return NULL;
    }

    if(LEXER_TOKEN_TYPE_INT_LITERAL == tokens[*cursor].type) {
        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, tokens[*cursor].line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_INT_LITERAL);

        if(t){
            node->data.literal_data = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(LEXER_TOKEN_TYPE_IDENTIFIER == tokens[*cursor].type){
        struct parser_node *node = parser_create_node(PARSER_NODE_IDENTIFIER, tokens[*cursor].line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        if(t){
            node->data.variable_name = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(LEXER_TOKEN_TYPE_LPAREN == tokens[*cursor].type){
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

        struct parser_node *node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        if(!eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)){
            C_LOG_ERR("expected \")\" on line: %d", tokens[*cursor].line);
            parser_delete_node(&node);
            (*cursor)+=1;
            return NULL;
        }

        return node;
    }else {
        C_LOG_ERR("parser_parse_factor - current token (%s) is not literal or identifier (unexpected token), line: %d", tokens[*cursor].token ,tokens[*cursor].line);
        return NULL;
    }
    return NULL;
}

struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(*cursor >= token_count) {LOG_M_ERR("parser_parse_unary - \"*cursor >= token_count\""); return NULL;}
    enum token_type tok_type = tokens[*cursor].type;
    if (LEXER_TOKEN_TYPE_MINUS == tok_type || LEXER_TOKEN_TYPE_PLUS == tok_type || LEXER_TOKEN_TYPE_BANG == tok_type) {
        int op_line = tokens[*cursor].line;

        struct lexer_token *op_token = &tokens[*cursor];
        (*cursor)++;

        struct parser_node *right_node = parser_parse_unary(parser, tokens, token_count, cursor);

        if (LEXER_TOKEN_TYPE_PLUS == op_token->type) {
            return right_node; 
        }

        struct parser_node *node = parser_create_node((LEXER_TOKEN_TYPE_MINUS == tok_type) ? PARSER_NODE_UNARY_MINUS : PARSER_NODE_UNARY_BANG, op_line);

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
