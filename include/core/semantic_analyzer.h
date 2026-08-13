#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "core/globals.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include "h_string_view.h"
#include <string.h>

struct semantic_context {
    struct parser_node *current_function;
    struct parser_node *current_block;
    struct symbol_table *current_scope;
    struct type_table *type_table;

    int error;
};

int semantic_analyzer_run_analyzer(struct parser_t *parser);
static int semantic_analyzer_analyze_node(struct parser_node* node, struct semantic_context* context);
static int semantic_analyzer_analyze_block(struct parser_node *node, struct semantic_context *context);
static int semantic_analyzer_analyze_function_decl(struct parser_node* node, struct semantic_context* context);
static int semantic_analyzer_analyze_var_declaration(struct parser_node* node, struct semantic_context* context);
static int semantic_analyzer_analyze_assigment(struct parser_node* node, struct semantic_context* context);
static int semantic_analyzer_analyze_call(struct parser_node* node, struct semantic_context* context);
static int semantic_analyzer_analyze_binary_expr(struct parser_node* node, struct semantic_context* context);

struct type_info *semantic_analyzer_calculate_type_infos(struct parser_node *node, struct semantic_context *context);

static void semantic_analyzer_propagate_literal_types(struct parser_node *node, struct type_info *target_type);

static inline int type_check(struct type_info *t1, struct type_info *t2) {
    if(NULL == t1 || NULL == t2) return -1;
    return str_view_eq(t1->name, t2->name);
}

#endif
