#ifndef IR_LOWER_H
#define IR_LOWER_H

#include "ir_gen.h"
#include "parser.h"
#include "symbol_table.h"

struct IR_Builder {
    struct IR_Module *module;
    struct IR_Function *current_function;
    struct IR_Block *current_block;
    struct symbol_table *current_scope;

    struct IR_Block *current_loop_header;
    struct IR_Block *current_loop_exit;
};

struct IR_Builder *IRL_create_IR_Builder();
int IRL_delete_IR_Builder(struct IR_Builder **builder);

struct IR_Module* IRLower_program(struct IR_Builder *restrict builder, struct parser_t *restrict parser);

struct IR_Operand *IRLower_block(struct IR_Builder *builder, struct parser_node *node);
struct IR_Operand *IRLower_statement(struct IR_Builder *builder, struct parser_node *node);
void IRLower_function(struct IR_Builder *builder, struct parser_node *node);
void IRLower_loop(struct IR_Builder *builder, struct parser_node *node);
void IRLower_break(struct IR_Builder *builder, struct parser_node *node);
void IRLower_continue(struct IR_Builder *builder, struct parser_node *node);

struct IR_Operand* IRLower_expression(struct IR_Builder *builder, struct parser_node *node);
struct IR_Operand* IRLower_binary_op(struct IR_Builder *builder, struct parser_node *node);
struct IR_Operand* IRLower_unary_op(struct IR_Builder *builder, struct parser_node *node);
struct IR_Operand* IRLower_address(struct IR_Builder *builder, struct parser_node *node);

struct IR_Operand *IRL_new_vreg(struct IR_Builder *builder);

#endif
