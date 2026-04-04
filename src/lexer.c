#include "lexer.h"
#include <stdlib.h>
#include <string.h>

const char LEXER_DELIM[] = " \t\r\n";

const char language_keywords[LEXER_KEYWORD_COUNT][LEXER_MAX_KEYWORD_CHAR_LENGHT] = {"return", "var", "for", "while", "loop"};
const char language_primitive_types[LEXER_PRIMITIVE_TYPE_COUNT][LEXER_MAX_PRIMITIVE_TYPE_CHAR_LENGHT] = {"int8", "int16", "int32", "int64", "char", "float", "void"};

int lexer_compare_keyword(const char *restrict word){ //Returns Keyword ID
    for(int i = 0;i < LEXER_KEYWORD_COUNT; ++i){
        const char *restrict keyword = language_keywords[i];
        if(strcmp(word, keyword) == 0) return i;
    }
    return -1;
}

int lexer_compare_primitive_type(const char *restrict word){ // Returns Primitive Type ID
    for(int i = 0;i < LEXER_PRIMITIVE_TYPE_COUNT; ++i){
        const char *restrict primitive_type = language_primitive_types[i];
        if(strcmp(word, primitive_type) == 0) return i;
    }
    return -1;
}

int lexer_create_lexer_line(struct lexer_line *restrict line, char *restrict str){
    line->tokens = malloc(sizeof(char *) * 64);
    line->tokens = memset(line->tokens, 0, sizeof(char *) * 64);
    line->char_count = strlen(str);
    int token_count = lexer_tokenize(str, line->tokens);
    line->token_count = token_count;
    return -1;
}

int lexer_tokenize(char *restrict str, struct lexer_token *restrict tokens){ //Returns token count
    int t_id = 0;

    char *token = strtok(str, LEXER_DELIM);
    while(token != NULL) {
        tokens[t_id].token = strdup(token);
        ////
        tokens[t_id].type = LEXER_TOKEN_TYPE_IDENTIFIER; // TODO: Remove after removing strtok
        ////
        if(tokens[t_id].token == NULL) return -1;

        t_id++;
        token = strtok(NULL, LEXER_DELIM);
    }
    return t_id;
}
