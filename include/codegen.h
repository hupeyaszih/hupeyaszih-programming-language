#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"
#include <stdio.h>

struct codegen_t{
    FILE *output_file;
    struct symbol_table *current_scope;
    struct type_table *type_table;
    struct parser_t *parser;
    int label_count;
};

struct codegen_t *codegen_create_codegen(struct parser_t *restrict parser, const char *restrict out_filename);
void codegen_generate(struct codegen_t *restrict codegen, struct parser_t *restrict parser, struct symbol_table *global_scope);
void codegen_pre_codegen_analysis(struct parser_node *node, struct symbol_table *current_scope);

void codegen_visit_node(struct codegen_t *restrict codegen, struct parser_node *restrict node, struct symbol_table *global_scope);
void codegen_delete_codegen(struct codegen_t *restrict cg);

void codegen_calculate_offsets(struct symbol_table *scope, int base_offset);
// void codegen_generate_asma(const struct parser_t *restrict parser); // Assembly Abstraction
//
// void codegen_generate_asm();  // Translate asma to Assembly
//
#endif
