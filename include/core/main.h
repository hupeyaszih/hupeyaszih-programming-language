#ifndef MAIN_H
#define MAIN_H

#include "core/ir_dumper.h"
#include "core/ir_lower.h"
#include "core/semantic_analyzer.h"
#include "globals.h"
#include "h_arena.h"
#include "lexer.h"
#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define M_FLAG_RUN (0)
#define M_FLAG_CLEAN (1)
#define M_FLAG_IR_DUMP (2)

struct main_context {
    struct arena symbol_arena;
    struct arena lexer_arena;
    struct arena parser_arena;
    struct arena ir_arena;
    struct arena codegen_arena;
    struct arena temp_arena;
};

struct main_context init_main(void) {
    struct main_context context;
    context.symbol_arena = arena_create();
    context.lexer_arena = arena_create();
    context.parser_arena = arena_create();
    context.ir_arena = arena_create();
    context.codegen_arena = arena_create();
    context.temp_arena = arena_create();
    return context;
}

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[], const char *file_name, int *build_successful, struct main_context *context){
    if(NULL == fl) {
        *build_successful = 0;
        return NULL;
    }

    LOG_M("Lexer started...");
    struct lexer_file *file = arena_alloc(&context->lexer_arena, sizeof(struct lexer_file));
    if(lexer_create_lexer_file(file, fl, file_name, &context->lexer_arena)) return NULL;
    LOG_M("line count: %d, statement count: %d", file->line_count, file->statement_count);
    
    // for(int i = 0;i < file->token_count; ++i){
    //     printf(SV_FMT " | token type: %s\n", SV_ARG(file->tokens[i].str_view), lexer_token_type_to_string((*(file->tokens+i)).type));
    // }
    
    LOG_M("total token count: %d", file->token_count);
    LOG_M("Parser started...");
    int parser_result = parser_parse(parser, file);
    if(parser_result){
        C_LOG_OK("Parser finished successfully");
        LOG_M("Semantic Analyzer started...");
        int semantic_err = semantic_analyzer_run_analyzer(parser);
        if(semantic_err){
            C_LOG_ERR("Semantic Analyzer failed");
            *build_successful = 0;
        }else {
            C_LOG_OK("Semantic Analyzer finished successfully");
        }
    }else {
        *build_successful = 0;
        C_LOG_ERR("Parser failed");
    }

    return file;
}

static inline void run_flag_func(const char *restrict build_path, struct IR_Project *project) {
    if (!project || !project->modules) return;

    size_t count = project->modules->element_count;

    size_t obj_capacity = 256;
    size_t obj_len = 0;
    char *obj_files = malloc(obj_capacity);
    if (!obj_files) {
        return;
    }
    obj_files[0] = '\0';

    for (size_t i = 0; i < count; ++i) {
        struct IR_Module **module_ptr = vector_get(project->modules, i);
        if (!module_ptr || !*module_ptr) continue;

        struct IR_Module *module = *module_ptr;
        char *name = module->name;
        if (!name) continue;

        char command[1024];
        int written = snprintf(command, sizeof(command), "as -o \"%s/%s.o\" \"%s/%s.s\"", build_path, name, build_path, name);
        
        if (written < 0 || (size_t)written >= sizeof(command)) {
            free(obj_files);
            return;
        }

        int res = system(command);
        if (res != 0) {
            free(obj_files);
            return;
        }

        size_t path_len = strlen(build_path) + strlen(name) + 6;        

        while (obj_len + path_len + 1 > obj_capacity) {
            obj_capacity *= 2;
            char *new_ptr = realloc(obj_files, obj_capacity);
            if (!new_ptr) {
                free(obj_files);
                return;
            }
            obj_files = new_ptr;
        }

        int added = snprintf(obj_files + obj_len, obj_capacity - obj_len, " \"%s/%s.o\"", build_path, name);
        if (added > 0) {
            obj_len += added;
        }
    }

    size_t link_cmd_len = strlen(build_path) + obj_len + 32;
    char *link_command = malloc(link_cmd_len);

    if (link_command) {
        snprintf(link_command, link_cmd_len, "ld -o \"%s/main\"%s", build_path, obj_files);
        
        int link_res = system(link_command);
        if (link_res != 0) {
            C_LOG_ERR("Linker: ld err\n");
        }
        free(link_command);
    }

    free(obj_files);
}


#endif
