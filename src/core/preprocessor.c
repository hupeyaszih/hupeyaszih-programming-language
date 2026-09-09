#include "core/preprocessor.h"
#include "h_arena.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stdio.h>

static inline struct macro *macro_table_find(struct macro_table *table, const struct str_view name) {
    for (int i = 0; i < table->macros->element_count; ++i) {
        struct macro *macro = (struct macro*) vector_get(table->macros, i);
        if (str_view_eq(macro->name, name)) {
            return macro;
        }
    }
    return NULL;
}

static void expand_macro(struct arena *arena, struct vector_t *output_tokens, struct macro *m, struct lexer_token *tokens, int *i, int token_count, struct macro_table *table) {
    int cursor = *i + 1;
    
    struct vector_t *args = NULL;
    if (m->param_count > 0) {
        if (cursor < token_count && tokens[cursor].type == LEXER_TOKEN_TYPE_LPAREN) {
            cursor++;
            
            args = vector_create_vector(arena, m->param_count, sizeof(struct vector_t));
            
            struct vector_t *current_arg = vector_create_vector(arena, 4, sizeof(struct lexer_token));
            int paren_depth = 0;
            
            while (cursor < token_count) {
                struct lexer_token *t = &tokens[cursor];
                
                if (t->type == LEXER_TOKEN_TYPE_LPAREN) {
                    paren_depth++;
                    vector_add(current_arg, t);
                } else if (t->type == LEXER_TOKEN_TYPE_RPAREN) {
                    if (paren_depth == 0) {
                        vector_add(args, current_arg);
                        cursor++;
                        break;
                    }
                    paren_depth--;
                    vector_add(current_arg, t);
                } else if (t->type == LEXER_TOKEN_TYPE_COMMA && paren_depth == 0) {
                    vector_add(args, current_arg);
                    current_arg = vector_create_vector(arena, 4, sizeof(struct lexer_token));
                } else {
                    vector_add(current_arg, t);
                }
                cursor++;
            }
        }
    }

    for (int b = 0; b < m->body_token_count; ++b) {
        struct lexer_token *body_token = &m->body_tokens[b];
        
        int param_index = -1;
        if (m->param_count > 0) {
            for (int p = 0; p < m->param_count; ++p) {
                if (str_view_eq(body_token->str_view, m->params[p].str_view)) {
                    param_index = p;
                    break;
                }
            }
        }
        
        if (param_index != -1 && args != NULL) {
            struct vector_t *arg_tokens = &((struct vector_t *)args->data)[param_index];
            for (int at = 0; at < arg_tokens->element_count; ++at) {
                struct lexer_token *arg_t = &((struct lexer_token *)arg_tokens->data)[at];
                vector_add(output_tokens, arg_t);
            }
        } else {
            vector_add(output_tokens, body_token);
        }
    }

    *i = cursor;
}

int preprocessor_parse_define(struct arena *arena, struct macro_table *table, struct lexer_token *tokens, int i, int token_count) {
    if (i + 1 >= token_count || !str_view_eq_cstr(tokens[i + 1].str_view, "define")) {
        return i + 1;
    }

    int macro_line = tokens[i].line;
    int cursor = i + 2;

    if (cursor >= token_count || tokens[cursor].type != LEXER_TOKEN_TYPE_IDENTIFIER) {
        return cursor;
    }

    struct str_view macro_name = tokens[cursor].str_view;
    cursor++;

    struct vector_t *params_vec = vector_create_vector(arena, 4, sizeof(struct lexer_token));
    int has_params = 0;
    if (cursor < token_count && tokens[cursor].type == LEXER_TOKEN_TYPE_LPAREN && tokens[cursor].line == macro_line) {
        int peek = cursor + 1;
        int is_valid_params = 1;
        int found_tokens = 0;

        while (peek < token_count && tokens[peek].type != LEXER_TOKEN_TYPE_RPAREN && tokens[peek].line == macro_line) {
            struct lexer_token *t = &tokens[peek];
            if (t->type != LEXER_TOKEN_TYPE_IDENTIFIER && t->type != LEXER_TOKEN_TYPE_COMMA) {
                is_valid_params = 0;
                break;
            }
            found_tokens++;
            peek++;
        }

        if (is_valid_params && (found_tokens == 0 || (peek < token_count && tokens[peek].type == LEXER_TOKEN_TYPE_RPAREN))) {
            has_params = 1;
            cursor++;

            while (cursor < token_count && tokens[cursor].type != LEXER_TOKEN_TYPE_RPAREN) {
                if (tokens[cursor].type == LEXER_TOKEN_TYPE_IDENTIFIER) {
                    vector_add(params_vec, &tokens[cursor]);
                }
                cursor++;
            }
            if (cursor < token_count && tokens[cursor].type == LEXER_TOKEN_TYPE_RPAREN) {
                cursor++;
            }
        }
    }

    struct vector_t *body_vec = vector_create_vector(arena, 16, sizeof(struct lexer_token));
    while (cursor < token_count && tokens[cursor].type != LEXER_TOKEN_TYPE_DOLLAR) {
        vector_add(body_vec, &tokens[cursor]);
        ++cursor;
    }
    ++cursor;

    struct macro m;
    m.name = macro_name;
    m.param_count = params_vec->element_count;
    m.params = params_vec->data;
    m.body_token_count = body_vec->element_count;
    m.body_tokens = body_vec->data;

    vector_add(table->macros, &m);
    return cursor;
}

void preprocessor_run(struct arena *arena, struct lexer_file *file) {
    struct macro_table *table = preprocessor_create_macro_table(arena);

    struct vector_t *clean_tokens = vector_create_vector(arena, file->token_count, sizeof(struct lexer_token));
    int i = 0;
    while (i < file->token_count) {
        struct lexer_token *curr = &file->tokens[i];
        if (curr->type == LEXER_TOKEN_TYPE_HASH) {
            i = preprocessor_parse_define(arena, table, file->tokens, i, file->token_count);
            continue;
        }
        vector_add(clean_tokens, curr);
        i++;
    }
    
    struct vector_t *current_tokens = clean_tokens;

    int max_iterations = 64;
    int iteration = 0;
    int expanded_any = 1;

    while (expanded_any && iteration < max_iterations) {
        expanded_any = 0;
        struct vector_t *output_tokens = vector_create_vector(arena, current_tokens->element_count, sizeof(struct lexer_token));
        
        int j = 0;
        while (j < current_tokens->element_count) {
            struct lexer_token *curr = &((struct lexer_token *)current_tokens->data)[j];

            struct macro *m = NULL;
            if (curr->type == LEXER_TOKEN_TYPE_IDENTIFIER) {
                m = macro_table_find(table, curr->str_view);
            }

            if (m) {
                expand_macro(arena, output_tokens, m, current_tokens->data, &j, current_tokens->element_count, table);
                expanded_any = 1;
            } else {
                vector_add(output_tokens, curr);
                j++;
            }
        }
        
        current_tokens = output_tokens;
        iteration++;
    }

    file->tokens = current_tokens->data;
    file->token_count = current_tokens->element_count;
}


struct macro_table *preprocessor_create_macro_table(struct arena *arena) {
    struct macro_table *table = arena_alloc(arena, sizeof(struct macro_table));
    table->macros = vector_create_vector(arena, 4, sizeof(struct macro));
    return table;
}

