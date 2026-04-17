#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

static inline struct lexer_file *lexer_test(struct parser_t *restrict parser, char fl[]){
    printf("\n\n\nfile: %s\n", fl);

    struct lexer_file *file = malloc(sizeof(struct lexer_file));
    lexer_create_lexer_file(file, fl);
    
    for(int i = 0;i < file->token_count; ++i){
        printf("%s | token type: %s\n", (*(file->tokens+i)).token, lexer_token_type_to_string((*(file->tokens+i)).type));
    }
    
    printf("total token count: %d\n", file->token_count);
    int result = parser_parse(parser, file);
    printf("parser result: %d\n", result);

    return file;
}

int main() {

    struct parser_t *parser = parser_create_parser();

    struct lexer_file *file_1 = lexer_test(parser, "-5 + (+10 * -2); -5; +5;\n 5*5-(10+7+(5*3));");

    parser_delete_parser(&parser);

    lexer_delete_lexer_file(file_1);
    return 0;
}
