#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "symbol_table.h"

enum parser_node_type {
    PARSER_NODE_EQUAL_EQUAL = 1,
    PARSER_NODE_BANG_EQUAL = 2,
    PARSER_NODE_LESS_EQUAL = 3,
    PARSER_NODE_GREATER_EQUAL = 4,
    PARSER_NODE_LESS = 5,
    PARSER_NODE_GREATER = 6,
    PARSER_NODE_UNARY_BANG,
    PARSER_NODE_UNARY_MINUS,
    PARSER_NODE_PLUS,
    PARSER_NODE_MINUS,
    PARSER_NODE_DIVIDE,
    PARSER_NODE_MUL,
    PARSER_NODE_NUMBER,
    PARSER_NODE_IDENTIFIER,
    PARSER_NODE_VARIABLE_DECLARATION,
    PARSER_NODE_VARIABLE_ASSIGMENT,
    PARSER_NODE_PARAMETER_LIST,
    PARSER_NODE_FUNCTION,
    PARSER_NODE_LOOP,
    PARSER_NODE_BREAK,
    PARSER_NODE_CONTINUE,
    PARSER_NODE_ASM,
    PARSER_NODE_CALL,
    PARSER_NODE_BLOCK,
    PARSER_NODE_UNDEFINED
};

struct parser_node{
   enum parser_node_type type;      // Node type
   struct parser_node *left_node;   // left sub node
   struct parser_node *right_node;  // right sub node

   union {
       char *variable_name;
       char *literal_data;

       struct {
           struct parser_node **statements;
           int count;
           struct symbol_table *scope;
       } block;

       struct {
           char *name;
           char *mangled_name;
           struct parser_node *params;
           int param_count;
           struct parser_node *body;
           struct type_info *return_type;
       } function;

       struct {
           char *mangled_name;
           struct parser_node *body;
           int loop_id;
       } loop;

       struct {
           char *mangled_loop_control_name;
           char *mangled_loop_name;
           struct parser_node *condition;
           struct parser_node *body;
           int loop_control_id;
           int loop_id;
       } loop_control;

       struct {
           char *name;
           struct parser_node **args;
           int arg_count;
       } call;

       struct {
           char *assembly_data;
       } assembly;
   } data;

   struct type_info *type_info;

   int line; 
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
    struct symbol_table *current_scope;
    struct type_table *type_table;
    int node_count;
    int scope_counter;
    int loop_depth_counter;
    int loop_id_counter;
    int loop_control_depth_counter;
    int loop_control_id_counter;

    int current_loop_id;
    int successful;
};

struct parser_t *parser_create_parser();
void parser_delete_parser(struct parser_t **parser);

struct parser_node *parser_create_node(enum parser_node_type type, int line);
void parser_delete_node(struct parser_node **node);

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node);

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file);
struct parser_node *parser_parse_statement(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_parse_parameters(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_function(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_loop(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_break(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_continue(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_asm(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_call(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, char *func_name);
struct parser_node *parser_parse_block(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor, int create_new_scope);
struct parser_node *parser_parse_variable_declaration(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_assignment(struct parser_t *parser, struct lexer_token *tokens, int token_count, int *cursor);
struct parser_node *parser_parse_boolean_logic(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_expression(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_term(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);
struct parser_node *parser_parse_factor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor);

int parser_parse_control_depth(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int cursor);

void parser_print_tree(struct parser_node *node, int depth);
#endif
