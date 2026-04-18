#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "globals.h"

const char LEXER_DELIM[] = " \t\r\n";

const char language_keywords[LEXER_KEYWORD_COUNT][LEXER_MAX_KEYWORD_CHAR_LENGHT] = {"return", "var", "for", "while", "loop"};
const char language_primitive_types[LEXER_PRIMITIVE_TYPE_COUNT][LEXER_MAX_PRIMITIVE_TYPE_CHAR_LENGHT] = {"int8", "int16", "int32", "int64", "char", "float", "void"};


int lexer_is_double_operator_token(const char *chr){
    if((chr+1) && '=' == (*(chr+1))) {
        switch (*chr) {
            case '=':
                return 0;
            case '!':
                return 0;
            case '<':
                return 0;
            case '>':
                return 0;
            default:
                break;
        }
    }
    return -1;
}

static inline enum token_type lexer_get_symbol_type(const char *chr) {
    if(0 == lexer_is_double_operator_token(chr)){
        switch (*chr) {
            case '=':
                return LEXER_TOKEN_TYPE_EQUAL_EQUAL;
            case '!':
                return LEXER_TOKEN_TYPE_BANG_EQUAL;
            case '<':
                return LEXER_TOKEN_TYPE_LESS_EQUAL;
            case '>':
                return LEXER_TOKEN_TYPE_GREATER_EQUAL;
            default:
                break;
        }
    }

    switch (*chr) {
        case '(':
            return LEXER_TOKEN_TYPE_LPAREN;
        case ')':
            return LEXER_TOKEN_TYPE_RPAREN;
        case '{':
            return LEXER_TOKEN_TYPE_LBRACE;
        case '}':
            return LEXER_TOKEN_TYPE_RBRACE;
        case '[':
            return LEXER_TOKEN_TYPE_LBRACKET;
        case ']':
            return LEXER_TOKEN_TYPE_RBRACKET;
        case ':':
            return LEXER_TOKEN_TYPE_COLON;
        case ';':
            return LEXER_TOKEN_TYPE_SEMICOLON;
        case '>':
            return LEXER_TOKEN_TYPE_GREATER;
        case '<':
            return LEXER_TOKEN_TYPE_LESS;
        case '=':
            return LEXER_TOKEN_TYPE_EQUAL;
        case '\"':
            return LEXER_TOKEN_TYPE_STRING_LITERAL;
        case ',':
            return LEXER_TOKEN_TYPE_COMMA;
        case '.':
            return LEXER_TOKEN_TYPE_DOT;
        case '+':
            return LEXER_TOKEN_TYPE_PLUS;
        case '-':
            return LEXER_TOKEN_TYPE_MINUS;
        case '*':
            return LEXER_TOKEN_TYPE_STAR;
        case '/':
            return LEXER_TOKEN_TYPE_SLASH;
        case '!':
            return LEXER_TOKEN_TYPE_BANG;
        case '%':
            return LEXER_TOKEN_TYPE_PERCENT;
        default:
            return LEXER_TOKEN_TYPE_UNKNOWN;
    }
}

static inline int lexer_is_number(const char chr) {
    if (chr >= '0' && chr <= '9') {
        return LEXER_TOKEN_TYPE_INT_LITERAL;
    }
    return -1;
}

static inline int lexer_is_keyword(const char *restrict token){
    for(int i = 0;i < LEXER_KEYWORD_COUNT; ++i){
        if(0 == strcmp(token, language_keywords[i])) return i;
    }
    
    for(int i = 0;i < LEXER_PRIMITIVE_TYPE_COUNT; ++i){
        if(0 == strcmp(token, language_primitive_types[i])) return LEXER_KEYWORD_COUNT + i;
    }
    return -1;
}


int lexer_compare_keyword(const char *restrict word){ //Returns Keyword ID
    for(int i = 0;i < LEXER_KEYWORD_COUNT; ++i){
        const char *restrict keyword = language_keywords[i];
        if(0 == strcmp(word, keyword)) return i;
    }
    return -1;
}

int lexer_compare_primitive_type(const char *restrict word){ // Returns Primitive Type ID
    for(int i = 0;i < LEXER_PRIMITIVE_TYPE_COUNT; ++i){
        const char *restrict primitive_type = language_primitive_types[i];
        if(0 == strcmp(word, primitive_type)) return i;
    }
    return -1;
}

static inline int calculate_line_count(const char *restrict str) {
    if (!str || '\0' == str[0]) return 0;

    int line_count = 1;
    const char *ptr = str;

    while (NULL != (ptr = strchr(ptr, '\n'))) {
        line_count++;
        ptr++; 
    }
    
    return line_count;
}

static inline int calculate_statement_count(const struct lexer_file *restrict file) {
    if (!file || file->token_count <= 0) return 0;

    int statement_count = 0;
    const struct lexer_token *tokens = file->tokens; 
    const int count = file->token_count;

    for (int i = 0; i < count; ++i) {
        if (LEXER_TOKEN_TYPE_SEMICOLON == tokens[i].type) {
            statement_count++;
        }
    }

    return statement_count;
}

int lexer_create_lexer_file(struct lexer_file *restrict file, char *restrict str){
    int current_capacity = 16;
    file->tokens = malloc(sizeof(struct lexer_token) * current_capacity);

    file->char_count = strlen(str);
    file->line_count = calculate_line_count(str);

    int token_count = lexer_tokenize(str, &file->tokens, &current_capacity);
    file->token_count = token_count;
    if(token_count <= 0) {
        C_LOG_ERR("lexer_create_lexer_file - \"token_count<=0\"\n");
        lexer_delete_lexer_file(file);
        return -1;
    }

    struct lexer_token *temp = realloc(file->tokens, sizeof(struct lexer_token) * (file->token_count + 1));
    if(temp){
        file->tokens = temp;
    }

    file->statement_count = calculate_statement_count(file);
    
    return 0;
}

void lexer_delete_lexer_file(struct lexer_file *restrict file){
   if (!file) return;
   if (file->tokens) {
       for (int i = 0; i < file->token_count; i++) {
           free(file->tokens[i].token);
       }
       free(file->tokens);
   }

   free(file);
}

int lexer_tokenize(char *restrict str, struct lexer_token **restrict tokens, int *current_token_capacity){ //Returns token count
    int line_index = 1;
    int i = 0;
    int token_id = 0;
    int str_len = strlen(str);

    while(i < str_len){
        if((*current_token_capacity) <= token_id){
            (*current_token_capacity) *= 2;
            struct lexer_token *temp = realloc((*tokens), sizeof(struct lexer_token) * (*current_token_capacity));
            if(temp){
                (*tokens) = temp;
            }else {
                return -1;
            }
        }
        char curr = str[i];
        if('\n' == curr) line_index++;
        if(isspace(curr)) {i++;continue;}
        if(isalpha(curr)){
            int start = i;
            while(i < str_len && (!isspace(str[i]) && LEXER_TOKEN_TYPE_UNKNOWN == lexer_get_symbol_type(&str[i])) ){
                i++;
            }

            (*tokens)[token_id].token = strndup(&str[start], i - start);
            (*tokens)[token_id].line = line_index;
            int is_keyword = lexer_is_keyword((*tokens)[token_id].token);

            if(-1 == is_keyword){
                (*tokens)[token_id].type = LEXER_TOKEN_TYPE_IDENTIFIER;
            }else {
                (*tokens)[token_id].type = LEXER_TOKEN_TYPE_KEYWORD;
            }

            token_id++;
            continue;
        }

        if(isdigit(curr)){
            int start = i;

            while (i < str_len && isdigit(str[i])) {
                i++;
            }

            (*tokens)[token_id].type = LEXER_TOKEN_TYPE_INT_LITERAL;
            (*tokens)[token_id].token = strndup(&str[start], i - start); 
            (*tokens)[token_id].line = line_index;
            token_id++;
            continue;
        }

        int symbol_type = lexer_get_symbol_type(&str[i]);
        int is_double_operator_token = (0 == lexer_is_double_operator_token(&str[i]));
        if (symbol_type != -1) {
            (*tokens)[token_id].type = symbol_type;
            (*tokens)[token_id].token = strndup(&str[i], 1 + is_double_operator_token);
            (*tokens)[token_id].line = line_index;
            token_id++;
            i += 1 + is_double_operator_token;
            continue;
        }

        i++;
    }

    return token_id;
}
