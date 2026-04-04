#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void lexer_test(char ln[]){
    printf("\nLine: %s\n", ln);
    struct lexer_line *line = malloc(sizeof(struct lexer_line));
    lexer_create_lexer_line(line, ln);

    for(int i = 0;i < line->token_count; ++i){
        printf("%s | token type: %s\n", (*(line->tokens+i)).token, lexer_token_type_to_string((*(line->tokens+i)).type));
    }

    free(line);
    printf("\n\n");
}

int main() {
    lexer_test("var year:int32 = 2026;");
    lexer_test("(a>123)b=45+67; @");
    return 0;
}

// TODO: remove strtok because it isn't enough
