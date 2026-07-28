#ifndef IR_LOWER_H
#define IR_LOWER_H

#include "core/ir_gen.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include "h_vector.h"

struct ir_context {
    struct IR_Project *project;
    struct IR_Function *current_function;
    struct IR_Block *current_block;

    struct symbol_table *current_scope;
    struct type_table *type_table;

    int error;
};

enum lower_type {
    LOWER_R,
    LOWER_L,
    LOWER_UNDEFINED,
};

struct IR_Operand *IRL_run_module_lower(struct parser_node *node, struct ir_context *context);
struct IR_Operand *IRL_run_function_lower(struct parser_node *node, struct ir_context *context);
struct IR_Operand *IRL_run_block_lower(struct parser_node *node, struct ir_context *context);
struct IR_Operand *IRL_run_statement_lower(struct parser_node *node, struct ir_context *context, enum lower_type lower_type);

int IRL_build_ir(struct IR_Project *project, struct parser_t *parser);

void IRL_find_mutations(struct parser_node *node, struct vector_t *vars, struct vector_t *declarated_vars);
#endif
