#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>


static inline char* err_to_string(int err_code){

    if(err_code == 0) {return "no error - successfull";}
    else if(err_code == -1000) {return "compiler error: \" if(file->token_count < *cursor) \"";}
    else if(err_code == -1) {return "expected \":\"";}
    else if(err_code == -2) {return "expected type (int32 etc.)";}
    else if(err_code == -3) {return "expected \"=\"";}
    else if(err_code == -4) {return "expected value";}
    else if(err_code == -5) {return "expected \";\"";}

    else if(err_code == -11) {return "expected \")\"";}
    return "Some Error";
}

char *parser_node_type_to_string(enum parser_node_type type){
    switch (type) {
        case PARSER_NODE_TYPE_NULL:
            return "NULL";
        case PARSER_NODE_TYPE_VAR_DECL:
            return "VAR_DECLARATION";
        case PARSER_NODE_TYPE_EXPRESSION:
            return "NODE_EXPRESSION";
    }
    return "UNKNOWN TYPE";
}

void parser_create_parser(struct parser_t **restrict parser, int node_count, int symbol_count){
    *parser = malloc(sizeof(struct parser_t));
    if(!parser) {parser = NULL; return;}

    (*parser)->node_count = 0;
    (*parser)->symbol_count = 0;

    (*parser)->nodes = malloc(sizeof(struct parser_node) * node_count);
    (*parser)->symbols = malloc(sizeof(struct parser_symbol) * symbol_count);
}
void parser_delete_parser(struct parser_t *restrict parser){
    for(int i = 0; i < parser->node_count; ++i){
        parser_delete_parser_node(parser->nodes[i]);
    }
    parser_delete_parser_symbol(parser->symbols);

    free(parser);
}

void parser_create_parser_symbol(struct parser_symbol **restrict symbol, char *name, char *type, void *value){
    *symbol = malloc(sizeof(struct parser_symbol));
    (*symbol)->type = type;
    (*symbol)->name = name;
    (*symbol)->value = value;
}
void parser_delete_parser_symbol(struct parser_symbol *restrict symbol){
    free(symbol->type);
    free(symbol->name);
    free(symbol->value);
    free(symbol);
}

void parser_add_sub_node(struct parser_node *main_node, struct parser_node *sub_node) {
    if(!main_node || !sub_node) return;

    struct parser_node **temp = realloc(main_node->sub_nodes, sizeof(struct parser_node*) * (main_node->sub_node_count + 1));

    if(!temp) return;

    main_node->sub_nodes = temp;
    main_node->sub_nodes[main_node->sub_node_count] = sub_node; 
    main_node->sub_node_count++;
}
void parser_create_parser_node(struct parser_node **node, const struct lexer_file *file, int start_index, int end_index){
    *node = malloc(sizeof(struct parser_node));
    if(!*node) return;
    (*node)->file = file;
    (*node)->start_index = start_index;
    (*node)->end_index = end_index;
    (*node)->sub_node_count = 0;
    (*node)->sub_nodes = NULL;
    (*node)->type = PARSER_NODE_TYPE_NULL;
}
void parser_delete_parser_node(struct parser_node *node){
    for(int i = 0; i < node->sub_node_count; ++i){
        parser_delete_parser_node(node->sub_nodes[i]);
    }
    free(node->sub_nodes);
    free(node);
}

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file){
    int cursor = 0;

    while(cursor < file->token_count){
        parser->node_count+=1;
        struct parser_node **main_node = &parser->nodes[parser->node_count-1];
        parser_create_parser_node(main_node, file, cursor, -1);

        if(file->tokens[cursor].type == LEXER_TOKEN_TYPE_KEYWORD){
            int res = parser_pars_var(parser, *main_node, file, &cursor);
            printf("%s\n", err_to_string(res));
            if(res != 0) return res;
        }else if(file->tokens[cursor].type == LEXER_TOKEN_TYPE_LPAREN){
            int res = parser_pars_expression(parser, *main_node, file, &cursor);
            printf("%s\n", err_to_string(res));
            if(res != 0) return res;
        }else{
            printf("error\n");
            return -1;
        }

        struct parser_node *n = (*main_node);
        while(n && n->sub_node_count > 0){
            n = n->sub_nodes[0];
            for(int i = n->start_index; i < n->end_index; ++i){
                if(i == -1 || i >= file->token_count) continue;
                printf("%s", file->tokens[i].token);
            }
            printf("\n");
        }
        cursor += 1;
    }

    return 0;
}

int parser_pars_var(struct parser_t *restrict parser, struct parser_node *restrict main_node, const struct lexer_file *restrict file, int *restrict cursor){
    main_node->type = PARSER_NODE_TYPE_VAR_DECL;
    int start_index = *cursor;

    *cursor+=1;

    if(file->token_count < *cursor) return -1000;
    if(file->tokens[*cursor].type != LEXER_TOKEN_TYPE_IDENTIFIER) return -1;
    *cursor+=1;

    if(file->token_count < *cursor) return -1000;
    if(file->tokens[*cursor].type != LEXER_TOKEN_TYPE_COLON) return -1;
    *cursor+=1;

    if(file->token_count < *cursor) return -1000;
    if(file->tokens[*cursor].type != LEXER_TOKEN_TYPE_KEYWORD || lexer_is_type(file->tokens[*cursor].token) == -1) return -2;
    *cursor+=1;

    if(file->token_count < *cursor) return -1000;
    if(file->tokens[*cursor].type != LEXER_TOKEN_TYPE_EQUAL) return -3;
    *cursor+=1;

    if(file->token_count < *cursor) return -1000;
    if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_LPAREN) {
        struct parser_node *sub_node;
        parser_create_parser_node(&sub_node, file, start_index, -1);
        parser_add_sub_node(main_node, sub_node);

        parser_pars_expression(parser, sub_node, file, cursor); 
        *cursor-=1;

        if(file->token_count < *cursor) return -1000;
        if(file->token_count < *cursor || file->tokens[*cursor].type != LEXER_TOKEN_TYPE_SEMICOLON) return -5;
        sub_node->end_index = *cursor;

        *cursor+=1;

        return 0;
    }
    else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_IDENTIFIER)       {*cursor += 1;}
    else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_INT_LITERAL)      {*cursor += 1;}
    else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_FLOAT_LITERAL)    {*cursor += 1;} 
    else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_STRING_LITERAL)   {*cursor += 1;} 
    else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_CHAR_LITERAL)     {*cursor += 1;}
    else {return -4;}

    if(file->token_count < *cursor) return -1000;
    if(file->token_count < *cursor || file->tokens[*cursor].type != LEXER_TOKEN_TYPE_SEMICOLON) return -5;
    *cursor+=1;

    return 0;
}

// int parser_pars_expression(struct parser_t *restrict parser, struct parser_node *restrict main_node, const struct lexer_file *restrict file, int *restrict cursor){
//     main_node->type = PARSER_NODE_TYPE_EXPRESSION;
//     struct parser_node *sub_node;
//     parser_create_parser_node(&sub_node, file, *cursor, -1);
//     parser_add_sub_node(main_node, sub_node);
//
//     int start_index = *cursor;
//     int end_index = -1;
//     *cursor += 1;
//     int depth = 1;
//     int i = *cursor;
//     while(depth > 0 && *cursor < file->token_count) {
//         if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_LPAREN) {depth++; parser_pars_expression(parser, sub_node, file, &i);}
//         else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_RPAREN) {depth--;}
//         if(depth == 0) {end_index = *cursor;}
//         *cursor += 1;
//     }
//
//     sub_node->end_index = *cursor;
//     *cursor += 1;
//
//     if(depth > 0) return -11;
//     return 0;
// }
int parser_pars_expression(struct parser_t *restrict parser, struct parser_node *restrict main_node, const struct lexer_file *restrict file, int *restrict cursor){
    main_node->type = PARSER_NODE_TYPE_EXPRESSION;
    struct parser_node *sub_node;
    parser_create_parser_node(&sub_node, file, *cursor, -1);
    parser_add_sub_node(main_node, sub_node);

    int start_index = *cursor;
    int end_index = -1;
    *cursor += 1;
    int depth = 1;
    int i = *cursor;
    while(depth > 0 && *cursor < file->token_count) {
        if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_LPAREN) {depth++; parser_pars_expression(parser, sub_node, file, &i);}
        else if(file->tokens[*cursor].type == LEXER_TOKEN_TYPE_RPAREN) {depth--;}
        if(depth == 0) {end_index = *cursor;}
        *cursor += 1;
    }

    sub_node->end_index = end_index;
    *cursor += 1;

    for(int i = start_index;i < end_index; ++i) {
        printf("%s", file->tokens[i].token);
    }
    printf("\n");

    if(depth > 0) return -11;
    return 0;
}
