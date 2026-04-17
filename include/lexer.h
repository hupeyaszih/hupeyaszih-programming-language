#ifndef LEXER_H
#define LEXER_H

#include <ctype.h>
#include <string.h>

enum token_type{
    LEXER_TOKEN_TYPE_EOF = 0,
    LEXER_TOKEN_TYPE_ERROR,
    LEXER_TOKEN_TYPE_UNKNOWN,

    LEXER_TOKEN_TYPE_IDENTIFIER,   
    LEXER_TOKEN_TYPE_INT_LITERAL, 
    LEXER_TOKEN_TYPE_FLOAT_LITERAL,
    LEXER_TOKEN_TYPE_STRING_LITERAL,
    LEXER_TOKEN_TYPE_CHAR_LITERAL,

    LEXER_TOKEN_TYPE_KEYWORD,

    LEXER_TOKEN_TYPE_PLUS,         // +
    LEXER_TOKEN_TYPE_MINUS,        // -
    LEXER_TOKEN_TYPE_STAR,         // *
    LEXER_TOKEN_TYPE_SLASH,        // /
    LEXER_TOKEN_TYPE_PERCENT,      // %
    LEXER_TOKEN_TYPE_EQUAL,        // =
    LEXER_TOKEN_TYPE_BANG,         // !

    LEXER_TOKEN_TYPE_EQUAL_EQUAL,  // ==
    LEXER_TOKEN_TYPE_BANG_EQUAL,   // !=
    LEXER_TOKEN_TYPE_LESS,         // <
    LEXER_TOKEN_TYPE_LESS_EQUAL,   // <=
    LEXER_TOKEN_TYPE_GREATER,      // >
    LEXER_TOKEN_TYPE_GREATER_EQUAL,// >=

    LEXER_TOKEN_TYPE_AND_AND,      // &&
    LEXER_TOKEN_TYPE_OR_OR,        // ||

    LEXER_TOKEN_TYPE_LPAREN,       // (
    LEXER_TOKEN_TYPE_RPAREN,       // )
    LEXER_TOKEN_TYPE_LBRACE,       // {
    LEXER_TOKEN_TYPE_RBRACE,       // }
    LEXER_TOKEN_TYPE_LBRACKET,     // [
    LEXER_TOKEN_TYPE_RBRACKET,     // ]
    LEXER_TOKEN_TYPE_COMMA,        // ,
    LEXER_TOKEN_TYPE_DOT,          // .
    LEXER_TOKEN_TYPE_SEMICOLON,    // ;
    LEXER_TOKEN_TYPE_COLON         // :
};

struct lexer_token{
    char *token;
    int line;
    enum token_type type;
};


struct lexer_file{
    int char_count;
    int token_count;

    int line_count; 
    int statement_count; // Ex: "4+3; 2=1; 3+6;" line_count = 1, statement_count = 3

    struct lexer_token *tokens;
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


int lexer_create_lexer_file(struct lexer_file *restrict file, char *restrict str);
void lexer_delete_lexer_file(struct lexer_file *restrict file);

int lexer_tokenize(char *restrict str, struct lexer_token **restrict tokens, int *current_token_capacity); //Returns token count

static const char* lexer_token_type_to_string(enum token_type type) {
    switch (type) {
        // Sistem ve Hata Tipleri
        case LEXER_TOKEN_TYPE_EOF:             return "EOF";
        case LEXER_TOKEN_TYPE_ERROR:           return "ERROR";
        case LEXER_TOKEN_TYPE_UNKNOWN:         return "UNKNOWN";

        // Tanımlayıcılar ve Literaller
        case LEXER_TOKEN_TYPE_IDENTIFIER:      return "IDENTIFIER";
        case LEXER_TOKEN_TYPE_INT_LITERAL:     return "INT_LITERAL";
        case LEXER_TOKEN_TYPE_FLOAT_LITERAL:   return "FLOAT_LITERAL";
        case LEXER_TOKEN_TYPE_STRING_LITERAL:  return "STRING_LITERAL";
        case LEXER_TOKEN_TYPE_CHAR_LITERAL:    return "CHAR_LITERAL";

        // Anahtar Kelimeler (Keywords)
        case LEXER_TOKEN_TYPE_KEYWORD:         return "KEYWORD";

        // Tek Karakterli Operatörler
        case LEXER_TOKEN_TYPE_PLUS:            return "PLUS";
        case LEXER_TOKEN_TYPE_MINUS:           return "MINUS";
        case LEXER_TOKEN_TYPE_STAR:            return "STAR";
        case LEXER_TOKEN_TYPE_SLASH:           return "SLASH";
        case LEXER_TOKEN_TYPE_PERCENT:         return "PERCENT";
        case LEXER_TOKEN_TYPE_EQUAL:           return "EQUAL";
        case LEXER_TOKEN_TYPE_BANG:            return "BANG";

        // Karşılaştırma Operatörleri
        case LEXER_TOKEN_TYPE_EQUAL_EQUAL:     return "EQUAL_EQUAL";
        case LEXER_TOKEN_TYPE_BANG_EQUAL:      return "BANG_EQUAL";
        case LEXER_TOKEN_TYPE_LESS:            return "LESS";
        case LEXER_TOKEN_TYPE_LESS_EQUAL:      return "LESS_EQUAL";
        case LEXER_TOKEN_TYPE_GREATER:         return "GREATER";
        case LEXER_TOKEN_TYPE_GREATER_EQUAL:   return "GREATER_EQUAL";

        // Mantıksal Operatörler
        case LEXER_TOKEN_TYPE_AND_AND:         return "AND_AND";
        case LEXER_TOKEN_TYPE_OR_OR:           return "OR_OR";

        // Ayraçlar ve Noktalama İşaretleri
        case LEXER_TOKEN_TYPE_LPAREN:          return "LPAREN";
        case LEXER_TOKEN_TYPE_RPAREN:          return "RPAREN";
        case LEXER_TOKEN_TYPE_LBRACE:          return "LBRACE";
        case LEXER_TOKEN_TYPE_RBRACE:          return "RBRACE";
        case LEXER_TOKEN_TYPE_LBRACKET:        return "LBRACKET";
        case LEXER_TOKEN_TYPE_RBRACKET:        return "RBRACKET";
        case LEXER_TOKEN_TYPE_COMMA:           return "COMMA";
        case LEXER_TOKEN_TYPE_DOT:             return "DOT";
        case LEXER_TOKEN_TYPE_SEMICOLON:       return "SEMICOLON";
        case LEXER_TOKEN_TYPE_COLON:           return "COLON";

        default:                               return "UNKNOWN_TOKEN";
    }
}

static inline int lexer_is_type(const char *restrict token){
    for(int i = 0;i < LEXER_PRIMITIVE_TYPE_COUNT; ++i){
        if(strcmp(token, language_primitive_types[i]) == 0) return LEXER_KEYWORD_COUNT + i;
    }
    return -1;
}
#endif

