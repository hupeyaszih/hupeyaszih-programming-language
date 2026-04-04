#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "globals.h"

const char LEXER_DELIM[] = " \t\r\n";

const char language_keywords[LEXER_KEYWORD_COUNT][LEXER_MAX_KEYWORD_CHAR_LENGHT] = {"return", "var", "for", "while", "loop"};
const char language_primitive_types[LEXER_PRIMITIVE_TYPE_COUNT][LEXER_MAX_PRIMITIVE_TYPE_CHAR_LENGHT] = {"int8", "int16", "int32", "int64", "char", "float", "void"};


static inline int lexer_is_keyword(const char *restrict token){
    for(int i = 0;i < LEXER_KEYWORD_COUNT; ++i){
        if(strcmp(token, language_keywords[i]) == 0) return i;
    }
    
    for(int i = 0;i < LEXER_PRIMITIVE_TYPE_COUNT; ++i){
        if(strcmp(token, language_primitive_types[i]) == 0) return LEXER_KEYWORD_COUNT + i;
    }
    return -1;
}

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
    int i = 0;
    int token_id = 0;
    int str_len = strlen(str);

    while(i < str_len){
        char curr = str[i];
        if(isspace(curr)) {i++;continue;}
        if(isalpha(curr)){
            int start = i;
            while(i < str_len && (!isspace(str[i]) && lexer_get_symbol_type(&str[i]) == LEXER_TOKEN_TYPE_UNKNOWN) ){
                i++;
            }

            tokens[token_id].token = strndup(&str[start], i - start);
            int is_keyword = lexer_is_keyword(tokens[token_id].token);

            if(is_keyword == -1){
                tokens[token_id].type = LEXER_TOKEN_TYPE_IDENTIFIER;
            }else {
                tokens[token_id].type = LEXER_TOKEN_TYPE_KEYWORD;
            }

            token_id++;
            continue;
        }

        if(isdigit(curr)){
            int start = i;

            while (i < str_len && isdigit(str[i])) {
                i++;
            }

            tokens[token_id].type = LEXER_TOKEN_TYPE_INT_LITERAL;
            tokens[token_id].token = strndup(&str[start], i - start); 
            token_id++;
            continue;
        }

        int symbol_type = lexer_get_symbol_type(&str[i]);
        int is_double_operator_token = (lexer_is_double_operator_token(&str[i]) == 0);
        if (symbol_type != -1) {
            tokens[token_id].type = symbol_type;
            tokens[token_id].token = strndup(&str[i], 1 + is_double_operator_token);
            token_id++;
            i += 1 + is_double_operator_token;
            continue;
        }

        i++;
    }

    return token_id;
}
