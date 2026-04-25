#ifndef MAIN_H
#define MAIN_H

#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include <stdlib.h>

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[], int *parser_error){
    // C_LOG("file: \n==========\n%s%s", fl, "==========");

    LOG_M("Lexer started...");
    struct lexer_file *file = malloc(sizeof(struct lexer_file));
    if(lexer_create_lexer_file(file, fl)) return NULL;
    LOG_M("line count: %d, statement count: %d", file->line_count, file->statement_count);
    
    // for(int i = 0;i < file->token_count; ++i){
    //     printf("%s | token type: %s\n", (*(file->tokens+i)).token, lexer_token_type_to_string((*(file->tokens+i)).type));
    // }
    
    LOG_M("total token count: %d", file->token_count);
    LOG_M("Parser started...");
    int result = parser_parse(parser, file);
    if(result){
        C_LOG_OK("Parser finished successfully");
        (*parser_error) = 0;
    }else {
        C_LOG_ERR("Parser failed");
        (*parser_error) = 1;
    }

    return file;
}

#endif
