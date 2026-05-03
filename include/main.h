#ifndef MAIN_H
#define MAIN_H

#include "globals.h"
#include "h_vector.h"
#include "ir_dumper.h"
#include "ir_gen.h"
#include "ir_lower.h"
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
    int result = parser_parse(parser, file);
    if(result){
        C_LOG_OK("Parser finished successfully");
    }else {
        C_LOG_ERR("Parser failed");
    }

    return file;
}

static inline void run_flag_func(const char *restrict file_path){
    char base_path[512];
    strncpy(base_path, file_path, sizeof(base_path));

    char *dot = strrchr(base_path, '.');
    if (dot != NULL && strcmp(dot, ".asm") == 0) {
        *dot = '\0';
    }

    char nasm_cmd[512];
    snprintf(nasm_cmd, sizeof(nasm_cmd), "nasm -f elf64 %s.asm -o %s.o", base_path, base_path);

    if (system(nasm_cmd) != 0) {
        LOG_M_ERR("NASM failed!\n");
        return;
    }

    char ld_cmd[512];
    snprintf(ld_cmd, sizeof(ld_cmd), "ld %s.o -o %s.bin", base_path, base_path);

    if (system(ld_cmd) != 0) {
        LOG_M_ERR("Linker (ld) failed!");
        return;
    }

    char run_cmd[512];
    snprintf(run_cmd, sizeof(run_cmd), "./%s.bin; echo \"\nProcess finished with exit code: $?\"", base_path);
    printf("--- Running Output ---\n");
    system(run_cmd);
}


static inline void ir_test(struct parser_t *parser, struct symbol_table *global_scope) {
    struct IR_Builder *builder = IRL_create_IR_Builder();
    struct IR_Module *module = IRLower_program(builder, parser);

    IR_dump_module(module);

    IRL_delete_IR_Builder(&builder);
    IR_delete_IR_Module(&module);
}



#endif
