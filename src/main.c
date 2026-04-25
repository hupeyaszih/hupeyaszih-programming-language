#include "main.h"
#include "codegen.h"
#include "hrs_file_io.h"
#include "parser.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    LOG_M_INFO("The compiler uses \"{}\" for internal compiler logs and \"[]\" for user-facing output");
    LOG_M_INFO("To disable internal compiler logs, undefine \"DEBUG\" in globals.h");
#ifndef DEBUG
    C_LOG_INFO("To enable internal compiler logs, define \"DEBUG\" in globals.h");
#endif

    struct parser_t *parser = parser_create_parser();

    struct symbol_table *global_scope = symbol_table_create_symbol_table(NULL, &parser->scope_counter);
    struct type_table *type_table = type_table_create_type_table();
    type_table_init_builtins(type_table);


    // Lexer & Parser
    parser->type_table = type_table;
    parser->current_scope = global_scope;

    const char file_path[] = "../example/testing.hrs";
    char *input = hrs_file_io_read_file("../example/testing.hrs");
    struct lexer_file *file = lexer_test(parser, input, file_path);


    struct codegen_t *codegen = codegen_create_codegen(parser, "build/testing.asm");
    if(parser->successful == 1){
        codegen_generate(codegen, parser, global_scope);
    }

    // for(int i = 0; i < parser->node_count; ++i){
    //     parser_print_tree(parser->nodes[i], parser->current_scope->scope_level);
    // }

    // Free
    codegen_delete_codegen(codegen);
    parser_delete_parser(&parser);
    symbol_table_delete_symbol_table(&global_scope);
    type_table_delete_type_table(&type_table);
    lexer_delete_lexer_file(file);
    free(input);

    return 0;
}
