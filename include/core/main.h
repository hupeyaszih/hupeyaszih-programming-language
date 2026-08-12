#ifndef MAIN_H
#define MAIN_H

#include "core/ir_dumper.h"
#include "core/ir_lower.h"
#include "core/semantic_analyzer.h"
#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[], const char *file_name, int *build_successful){
    if(NULL == fl) {
        *build_successful = 0;
        return NULL;
    }

    LOG_M("Lexer started...");
    struct lexer_file *file = malloc(sizeof(struct lexer_file));
    if(lexer_create_lexer_file(file, fl, file_name)) return NULL;
    LOG_M("line count: %d, statement count: %d", file->line_count, file->statement_count);
    
    // for(int i = 0;i < file->token_count; ++i){
    //     printf("%s | token type: %s\n", (*(file->tokens+i)).token, lexer_token_type_to_string((*(file->tokens+i)).type));
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
    char command[1024];
    
    char obj_files[2048] = "";

    for (int i = 0; i < count; ++i) {
        struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
        char *name = module->name;

        snprintf(command, sizeof(command), "as -o %s/%s.o %s/%s.s", 
                 build_path, name, build_path, name);
        
        int res = system(command);
        if (res != 0) {
            fprintf(stderr, "Hata: %s.s dosyasi derlenemedi (as hatasi).\n", name);
            return;
        }

        strcat(obj_files, " ");
        strcat(obj_files, build_path);
        strcat(obj_files, "/");
        strcat(obj_files, name);
        strcat(obj_files, ".o");
    }

    snprintf(command, sizeof(command), "ld -o %s/main %s", build_path, obj_files);
    
    int link_res = system(command);
    if (link_res != 0) {
        C_LOG_ERR("Linker: ld err\n");
    } 
}


static inline void ir_test(struct parser_t *parser, struct symbol_table *global_scope) {
}



#endif
