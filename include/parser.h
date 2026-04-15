#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

struct parser_symbol {
    char *name;
    char *type;
    void *value;
};

enum parser_node_type{
    PARSER_NODE_TYPE_NULL,
    PARSER_NODE_TYPE_VAR_DECL,
    PARSER_NODE_TYPE_EXPRESSION
};

struct parser_node{
    const struct lexer_file *file;
    int start_index, end_index;
    enum parser_node_type type;

    int sub_node_count;   
    struct parser_node **sub_nodes;
};

struct parser_t{
    struct parser_symbol *symbols;
    struct parser_node **nodes;

    int symbol_count;
    int node_count;
};

void parser_create_parser(struct parser_t **restrict parser, int node_count, int symbol_count);
void parser_delete_parser(struct parser_t *restrict parser);

void parser_create_parser_symbol(struct parser_symbol **restrict symbol, char *name, char *type, void *value);
void parser_delete_parser_symbol(struct parser_symbol *restrict symbol);

void parser_add_sub_node(struct parser_node *restrict main_node, struct parser_node *restrict sub_node);
void parser_create_parser_node(struct parser_node **node, const struct lexer_file *file, int start_index, int end_index);
void parser_delete_parser_node(struct parser_node *node);

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file);
int parser_pars_var(struct parser_t *restrict parser, struct parser_node *restrict main_node, const struct lexer_file *restrict file, int *restrict cursor);
int parser_pars_expression(struct parser_t *restrict parser, struct parser_node *restrict main_node, const struct lexer_file *restrict file, int *restrict cursor);
// int parser_pars_var(struct lexer_file *restrict file, int cursor);

char *parser_node_type_to_string(enum parser_node_type type);
#endif
