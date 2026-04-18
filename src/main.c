#include "globals.h"
#include "hrs_file_io.h"
#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[]){
    C_LOG("file: \n==========\n%s%s", fl, "==========");

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
    }else {
        C_LOG_ERR("Parser failed");
    }

    return file;
}

int main() {
    LOG_M_INFO("The compiler uses \"{}\" for internal compiler logs and \"[]\" for user-facing output");
    LOG_M_INFO("To disable internal compiler logs, undefine \"DEBUG\" in globals.h");
    LOG_M_INFO("Variable support is not enabled");
#ifndef DEBUG
    C_LOG_INFO("To enable internal compiler logs, define \"DEBUG\" in globals.h");
#endif

    struct parser_t *parser = parser_create_parser();

    char *input = hrs_file_io_read_file("../example/testing.hrs");
    struct lexer_file *file_2 = lexer_test(parser, input);

    parser_delete_parser(&parser);

    lexer_delete_lexer_file(file_2);
    return 0;
}
