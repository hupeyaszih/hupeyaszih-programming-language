#include "main.h"
#include "hrs_file_io.h"
#include "symbol_table.h"
#include <stdio.h>

int main() {
    LOG_M_INFO("The compiler uses \"{}\" for internal compiler logs and \"[]\" for user-facing output");
    LOG_M_INFO("To disable internal compiler logs, undefine \"DEBUG\" in globals.h");
#ifndef DEBUG
    C_LOG_INFO("To enable internal compiler logs, define \"DEBUG\" in globals.h");
#endif

    // Symbol Table & Type Table
    struct symbol_table *global_scope = symbol_table_create_symbol_table(NULL);
    struct type_table *type_table = type_table_create_type_table();
    type_table_init_builtins(type_table);


    // Lexer & Parser
    struct parser_t *parser = parser_create_parser();
    parser->type_table = type_table;
    parser->current_scope = global_scope;

    char *input = hrs_file_io_read_file("../example/testing.hrs");
    struct lexer_file *file_2 = lexer_test(parser, input);



    for(int i = 0; i < parser->node_count; ++i){
        struct eval_result res = parser_eval(parser->nodes[i], type_table, global_scope);
        if(NULL != res.type) {
            if(0 == strcmp(res.type->name, "int32")){
                C_LOG_OK("Result: %d", *(int*)res.raw_data);
            }
            if(res.raw_data) free(res.raw_data);
        }
    }

    // Free
    symbol_table_delete_symbol_table(&global_scope);
    type_table_delete_type_table(&type_table);
    parser_delete_parser(&parser);
    lexer_delete_lexer_file(file_2);

    return 0;
}
