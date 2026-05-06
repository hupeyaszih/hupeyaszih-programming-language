#include "core/main.h"
#include "core/globals.h"
#include "core/hrs_file_io.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include <stdlib.h>
#include <string.h>



int main(int argc, char *argv[]) {
    C_LOG_INFO("Usage: %s [-i input] [-o output] [--run] [--clean]", argv[0]);

    char* input_path = "../example/testing.hrs";
    char* output_path = "../out/main.asm";
    int run_flag = 0;
    int clean_flag = 0;
    for(int i = 1; i < argc; ++i) {
        if(0 == strcmp("-o", argv[i])) {
            if(i + 1 < argc) {
                output_path = argv[i+1];
                i++;
            }else {
                C_LOG_ERR("\"-o\" requires an output filename.");
                return 1;
            }

        }else if(0 == strcmp("-i", argv[i])) {
            if(i + 1 < argc) {
                input_path = argv[i+1];
                i++;
            }else {
                C_LOG_ERR("\"-i\" requires an input filename.");
                return 1;
            }
        }else if(0 == strcmp("--run", argv[i])) {
            run_flag = 1;
        }else if(0 == strcmp("--clean", argv[i])) {
            clean_flag = 1;
            system("rm -rf ../out/*.o ../out/*.bin ../out/*.asm"); 
            C_LOG_INFO("Build directory cleaned");
        }
    }

    LOG_M_INFO("The compiler uses \"{}\" for internal compiler logs and \"[]\" for user-facing output");
    LOG_M_INFO("To disable internal compiler logs, undefine \"DEBUG\" in globals.h");
#ifndef DEBUG
    C_LOG_INFO("To enable internal compiler logs, define \"DEBUG\" in globals.h");
#endif

    int build_successful = 1;

    struct parser_t *parser = parser_create_parser();

    struct symbol_table *global_scope = symbol_table_create_symbol_table(NULL, &parser->scope_counter);
    struct type_table *type_table = type_table_create_type_table();
    type_table_init_builtins(type_table);


    // Lexer & Parser
    parser->type_table = type_table;
    parser->current_scope = global_scope;

    char *input = hrs_file_io_read_file(input_path);
    struct lexer_file *file = lexer_test(parser, input, input_path, &build_successful);
    build_successful *= parser->successful;

    struct codegen_t *codegen = NULL;
    if(1 == build_successful){
        // codegen = codegen_create_codegen(parser, output_path);
        //
        // codegen_generate(codegen, parser, global_scope);
        // if(NULL != codegen) {
        //     build_successful *= codegen->successful;
        // }else {
        //     build_successful = 0;
        // }


        ir_test(parser, global_scope);
    }


    // Free
    // codegen_delete_codegen(&codegen);
    parser_delete_parser(&parser);
    symbol_table_delete_symbol_table(&global_scope);
    type_table_delete_type_table(&type_table);
    lexer_delete_lexer_file(file);
    free(input);

    if(1 == run_flag && 1 == build_successful) {
        run_flag_func(output_path);
    }

    return 0;
}
