#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

enum parser_node_type {
    PARSER_NODE_PLUS,
    PARSER_NODE_MINUS,
    PARSER_NODE_DIVIDE,
    PARSER_NODE_MUL,
    PARSER_NODE_NUMBER,
    PARSER_NODE_UNARY,
    PARSER_NODE_IDENTIFIER,
    PARSER_NODE_UNDEFINED
};

struct parser_node{
   enum parser_node_type type;      // Node type
   struct parser_node *left_node;   // left sub node
   struct parser_node *right_node;  // right sub node

   char* value; // literal or variable name
   int line; // Line which our node points at (for debugging)
};

static inline int is_node_type_operator(struct parser_node *restrict node){
    switch (node->type) {
        case PARSER_NODE_PLUS:
            return 0;
        case PARSER_NODE_MINUS:
            return 0;
        case PARSER_NODE_DIVIDE:
            return 0;
        case PARSER_NODE_MUL:
            return 0;
        default:
            return -1;
    }
    return -1;
}

struct parser_t{
    struct parser_node **nodes;
    int node_count;
};

struct parser_t *parser_create_parser();
void parser_delete_parser(struct parser_t **parser);

struct parser_node *parser_create_node(enum parser_node_type type, int line);
void parser_delete_node(struct parser_node **node);

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node);

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file);
struct parser_node *parser_parse_expression(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_term(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_factor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);

int parser_parse_control_depth(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int cursor);

#endif
