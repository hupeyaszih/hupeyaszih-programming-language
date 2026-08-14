#include "core/main.h"
#include "backend/codegen.h"
#include "core/globals.h"
#include "core/hrs_file_io.h"
#include "core/ir_dumper.h"
#include "core/ir_lower.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include "h_arena.h"
#include "h_bitset.h"
#include "opt/opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help() {
    C_LOG_INFO("to see run options, run with '-help_options'. To see available build targets, run with '-help_build_targets'");
}

void print_help_options() {
    C_LOG_INFO("Usage: [-i input] [-o output] [-target build_target_name] [-O0/O1/O2/O3] [--run] [--clean] [--ir_dump] [-help] [-help_options] [-help_build_targets]");
}

void print_help_build_targets() {
    C_LOG_INFO("\nBuild targets:\n - x86_64_linux\n");
}
void clean_build_directory(void) {
#ifdef _WIN32
    system("if exist ..\\out rmdir /s /q ..\\out && mkdir ..\\out");
#else
    system("rm -rf ../out/*");
#endif
}

int main(int argc, char *argv[]) {
    const int FLAG_COUNT = 8;
    C_LOG_INFO("to take help, run with '-help' flag");
    C_LOG_INFO("opt level is O0 by default, to switch among optimization levels please run with '-O0/1/2/3' !");

    char *input_path = "../example/example_00.hrs";
    char *output_path = "../out/";
    char *build_target = "x86_64_linux";
    enum optimization_level opt_level = OPT_LEVEL_UNDEFINED;

    struct arena main_arena = arena_create();
    struct bitset_t *flags = bitset_create(&main_arena, FLAG_COUNT);

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
        }else if(0 == strcmp("-target", argv[i])) {
            if(i + 1 < argc) {
                
                build_target = argv[i+1];
                i++;
            }else {
                C_LOG_ERR("\"-target\" requires a build target name.");
                return 1;
            }
        }else if(0 == strcmp("-O0", argv[i])) {
            if(opt_level != OPT_LEVEL_UNDEFINED) continue;
            opt_level = OPT_LEVEL_O0;
        }else if(0 == strcmp("-O1", argv[i])) {
            if(opt_level != OPT_LEVEL_UNDEFINED) continue;
            opt_level = OPT_LEVEL_O1;
        }else if(0 == strcmp("-O2", argv[i])) {
            if(opt_level != OPT_LEVEL_UNDEFINED) continue;
            opt_level = OPT_LEVEL_O2;
        }else if(0 == strcmp("-O3", argv[i])) {
            if(opt_level != OPT_LEVEL_UNDEFINED) continue;
            opt_level = OPT_LEVEL_O3;
        }else if(0 == strcmp("--run", argv[i])) {
            bitset_set(flags, M_FLAG_RUN);
        }else if(0 == strcmp("--clean", argv[i])) {
            bitset_set(flags, M_FLAG_CLEAN);
            clean_build_directory();
            C_LOG_INFO("Build directory cleaned");
        }else if(0 == strcmp("--ir_dump", argv[i])) {
            bitset_set(flags, M_FLAG_IR_DUMP);
        }else if(0 == strcmp("-help", argv[i])) {
            print_help();
            goto clean_1;
        }else if(0 == strcmp("-help_options", argv[i])) {
            print_help_options();
            goto clean_1;
        }else if(0 == strcmp("-help_build_targets", argv[i])) {
            print_help_build_targets();
            goto clean_1;
        }
    }

    LOG_M_INFO("The compiler uses \"{}\" for internal compiler logs and \"[]\" for user-facing output");
    LOG_M_INFO("To disable internal compiler logs, undefine \"DEBUG\" in globals.h");
#ifndef DEBUG
    C_LOG_INFO("To enable internal compiler logs, define \"DEBUG\" in globals.h");
#endif

    struct main_context context = init_main();
    int build_successful = 1;


    struct IR_Project *project = NULL;
    struct parser_t *parser = parser_create_parser(&context.parser_arena, &context.symbol_arena);

    struct symbol_table *global_scope = symbol_table_create_symbol_table(&context.symbol_arena, NULL, &parser->scope_counter);
    struct type_table *type_table = type_table_create_type_table(&context.symbol_arena);
    type_table_init_builtins(type_table);


    // Lexer & Parser
    parser->type_table = type_table;
    parser->current_scope = global_scope;

    char *input = hrs_file_io_read_file(input_path);
    struct lexer_file *file = lexer_test(parser, input, input_path, &build_successful, &context);
    build_successful *= parser->successful;

    struct codegen_t *codegen = codegen_create_codegen(&context.codegen_arena, &context.temp_arena, output_path);
    codegen_init_build_targets(codegen);

    codegen->current_build_target = *(struct codegen_build_target_t **)vector_get(codegen->build_targets, 0);

    if(1 == build_successful){
        project = IR_create_IR_Project(&context.ir_arena, &context.temp_arena);
        IRL_build_ir(project, parser);



        bool ir_dump_flag = bitset_test(flags, M_FLAG_IR_DUMP);

        opt_optimize_project(project, codegen, opt_level);
        if(ir_dump_flag) {
            for(int i = 0;i < project->modules->element_count; ++i) {
                struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
                IR_dump_module(module);
            }
        }

        codegen_build_project(codegen, project, build_target);
    }

    if(1 == bitset_test(flags, M_FLAG_RUN) && 1 == build_successful) {
        run_flag_func(output_path, project);
    }

    // Free
clean_0:
    free(input);
    

clean_1:
    arena_destroy(&context.temp_arena);
    arena_destroy(&context.symbol_arena);
    arena_destroy(&context.ir_arena);
    arena_destroy(&context.codegen_arena);
    arena_destroy(&context.parser_arena);
    arena_destroy(&context.lexer_arena);
    arena_destroy(&main_arena);


    return 0;
}
