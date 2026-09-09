#ifndef H_PREPROCESSOR_H
#define H_PREPROCESSOR_H

#include "core/lexer.h"

struct macro {
    struct str_view name;
    struct lexer_token *params;
    int param_count;
    struct lexer_token *body_tokens;
    int body_token_count;
};

struct macro_table {
    struct vector_t *macros; // struct macro *
};

void preprocessor_run(struct arena *arena, struct lexer_file *file);

struct macro_table *preprocessor_create_macro_table(struct arena *arena);

#endif
