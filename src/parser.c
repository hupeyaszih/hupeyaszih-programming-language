#include "parser.h"
#include "globals.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>


static inline double parser_eval(struct parser_node *node) { // For testing
    if (node == NULL) return 0.0;

    if (node->type == PARSER_NODE_NUMBER) {
        return atoi(node->value); 
    }

    if (node->type == PARSER_NODE_IDENTIFIER) {
        printf("Warning: Variable support is not enabled: %s\n", node->value);
        return 0.0;
    }

    double left_val = parser_eval(node->left_node);
    double right_val = parser_eval(node->right_node);

    switch (node->type) {
        case PARSER_NODE_PLUS:   return left_val + right_val;
        case PARSER_NODE_MINUS:  return left_val - right_val;
        case PARSER_NODE_MUL:    return left_val * right_val;
        case PARSER_NODE_DIVIDE: 
            if (right_val == 0.0) {
                printf("ERR: Cannot divide 0 by 0!\n");
                return 0.0;
            }
            return left_val / right_val;
        default:
            return 0.0;
    }
}

struct parser_t *parser_create_parser(){
    struct parser_t *parser = malloc(sizeof(struct parser_t));
    if(!parser) {LOG_ERR("parser_create_parser"); free(parser); return NULL;}
    parser->node_count = 0;
    parser->nodes = NULL;

    return parser;
}
void parser_delete_parser(struct parser_t **parser){
    if(parser == NULL || *parser == NULL) {LOG_ERR("parser_delete_parser - \"struct parser_t **parser\" or \"*parser\" is null\n"); return;}
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
    if(node == NULL || (*node) == NULL) {LOG_ERR("parser_delete_node - \"struct parser_node *node\" is null\n"); return;}
    if((*node)->left_node) parser_delete_node(&((*node)->left_node));
    if((*node)->right_node) parser_delete_node(&((*node)->right_node));

    free((*node));
    *node = NULL;
}

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node){
    if(node == NULL) {LOG_ERR("parser_parser_add_node - \"struct parser_node *node\" is null\n"); return;}
    if(parser == NULL) {LOG_ERR("parser_parser_add_node - \"struct parser_t *parser\" is null\n"); return;}

    struct parser_node **tmp = realloc(parser->nodes, sizeof(struct parser_node *) * (parser->node_count+1));
    if(!tmp) {LOG_ERR("parser_parser_add_node - couldn't realloc \"parser->nodes\""); return;}
    tmp[parser->node_count] = node;
    parser->nodes = tmp;
    parser->node_count += 1;
    return;
}

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file, int line){
    int cursor = 0;

    struct parser_node *node = parser_parse_expression(parser, file->tokens, file->token_count, &cursor, line);
    parser_parser_add_node(parser, node);

    printf("Result: %f\n", parser_eval(node)); // Debug

    return 0;
}

static inline struct lexer_token* eat(struct lexer_token *tokens, int token_count, int *cursor, enum token_type expected_type) {
    if (*cursor >= token_count || tokens[*cursor].type != expected_type) {
        printf("ERROR: unexpected token type!\n");
        return NULL;
    }
    return &tokens[(*cursor)++];
}


struct parser_node *parser_parse_expression(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int line){
    if(parser == NULL) {LOG_ERR("parser_parse_expression - \"struct parser_t *restrict parser\" is null")return NULL;}
    struct parser_node *left = parser_parse_term(parser, tokens, token_count, cursor, line);

    while(*cursor < token_count && (tokens[*cursor].type == LEXER_TOKEN_TYPE_PLUS || tokens[*cursor].type == LEXER_TOKEN_TYPE_MINUS)) {
        enum parser_node_type op_type = PARSER_NODE_MINUS;
        if(tokens[*cursor].type == LEXER_TOKEN_TYPE_PLUS) op_type = PARSER_NODE_PLUS;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_term(parser, tokens, token_count, cursor, line);
        if(!right){
            LOG_ERR("parser_parse_expression - \"struct parser_node *right\" is null");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, line);
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_term(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int line){
    if(parser == NULL) {LOG_ERR("parser_parse_term - \"struct parser_t *restrict parser\" is null")return NULL;}
    struct parser_node *left = parser_parse_factor(parser, tokens, token_count, cursor, line);

    while(*cursor < token_count && (tokens[*cursor].type == LEXER_TOKEN_TYPE_STAR || tokens[*cursor].type == LEXER_TOKEN_TYPE_SLASH)) {
        enum parser_node_type op_type = PARSER_NODE_DIVIDE;
        if(tokens[*cursor].type == LEXER_TOKEN_TYPE_STAR) op_type = PARSER_NODE_MUL;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_factor(parser, tokens, token_count, cursor, line);
        if(!right){
            LOG_ERR("parser_parse_term - \"struct parser_node *right\" is null");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, line);
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_factor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int line){
    if(parser == NULL) {LOG_ERR("parser_parse_factor - \"struct parser_t *restrict parser\" is null")return NULL;}
    if(*cursor >= token_count) {
        printf("expected \";\" on line: %d\n", line);
    }

    if(tokens[*cursor].type == LEXER_TOKEN_TYPE_INT_LITERAL) {
        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_INT_LITERAL);

        if(t){
            node->value = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_IDENTIFIER){
        struct parser_node *node = parser_create_node(PARSER_NODE_IDENTIFIER, line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        if(t){
            node->value = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_LPAREN){
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

        struct parser_node *node = parser_parse_expression(parser, tokens, token_count, cursor, line);
        if(!eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)){
            printf("expected \")\" on line: %d\n", line);
            parser_delete_node(&node);
            (*cursor)+=1;
            return NULL;
        }
        return node;
    }else {
        LOG_ERR("parser_parse_factor - current token is not literal or identifier (unexpected token)");
    }
    return NULL;
}
