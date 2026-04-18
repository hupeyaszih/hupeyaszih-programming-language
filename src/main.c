#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include <stdlib.h>

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[]){
    printf("\n________________________\n\nfile: %s\n", fl);

    LOG_M("Lexer started...\n");
    struct lexer_file *file = malloc(sizeof(struct lexer_file));
    lexer_create_lexer_file(file, fl);
    LOG_M("line count: %d, statement count: %d\n", file->line_count, file->statement_count);
    
    // for(int i = 0;i < file->token_count; ++i){
    //     printf("%s | token type: %s\n", (*(file->tokens+i)).token, lexer_token_type_to_string((*(file->tokens+i)).type));
    // }
    
    LOG_M("total token count: %d\n", file->token_count);
    LOG_M("Parser started...\n");
    int result = parser_parse(parser, file);
    C_LOG("parser result: %d\n", result);

    return file;
}

int main() {

    struct parser_t *parser = parser_create_parser();

    struct lexer_file *file_2 = lexer_test(parser, "(5 + (10 * (2 == 3)));");

    parser_delete_parser(&parser);

    lexer_delete_lexer_file(file_2);
    return 0;
}
