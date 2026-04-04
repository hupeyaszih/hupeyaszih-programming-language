#ifndef LEXER_H
#define LEXER_H

enum TokenType{
    LEXER_TOKEN_TYPE_KEYWORD,
    LEXER_TOKEN_TYPE_IDENTIFIER,
    LEXER_TOKEN_TYPE_NUMBER,
    LEXER_TOKEN_TYPE_OPERATOR
};

struct lexer_token{
    int size;
    char *token;
    enum TokenType type;
};

struct lexer_line{
    int char_count;
    int token_count;
    
    struct lexer_token *tokens;
};

struct lexer_file{
    int line_count;
    int char_count;
    int token_count;

    struct lexer_line *lines;
};

#define LEXER_MAX_KEYWORD_CHAR_LENGHT 10
#define LEXER_MAX_PRIMITIVE_TYPE_CHAR_LENGHT 10

#define LEXER_KEYWORD_COUNT 5
#define LEXER_PRIMITIVE_TYPE_COUNT 7

extern const char LEXER_DELIM[];

extern const char language_keywords[LEXER_KEYWORD_COUNT][LEXER_MAX_KEYWORD_CHAR_LENGHT];
extern const char language_primitive_types[LEXER_PRIMITIVE_TYPE_COUNT][LEXER_MAX_PRIMITIVE_TYPE_CHAR_LENGHT];

int lexer_compare_keyword(const char *restrict word);
int lexer_compare_primitive_type(const char *restrict word);

int lexer_create_lexer_line(struct lexer_line *restrict line, char *restrict str);

int lexer_tokenize(char *restrict str, struct lexer_token *restrict tokens);

#endif
