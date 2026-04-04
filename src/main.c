#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    struct lexer_line *line = malloc(sizeof(struct lexer_line));
    char str[] = "var year: int16 = 2026;";
    lexer_create_lexer_line(line, str);

    for(int i = 0;i < line->token_count; ++i){
        printf("%s\n", (*(line->tokens+i)).token);
    }
    return 0;
}

// TODO: remove strtok because it isn't enough
