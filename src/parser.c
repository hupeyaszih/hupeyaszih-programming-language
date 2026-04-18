#include "parser.h"
#include "globals.h"
#include "lexer.h"
#include <stdlib.h>


static inline double parser_eval(struct parser_node *node) { // For testing
    if (node == NULL) return 0.0;

    if (node->type == PARSER_NODE_NUMBER) {
        return atoi(node->value); 
    }

    if (node->type == PARSER_NODE_IDENTIFIER) {
        C_LOG_WARN("Variable support is not enabled: %s\n", node->value);
        return 0.0;
    }

    if (node->type == PARSER_NODE_UNARY_BANG) {
        return !parser_eval(node->right_node);
    }else if (node->type == PARSER_NODE_UNARY_MINUS) {
        return -parser_eval(node->right_node);
    }

    double left_val = parser_eval(node->left_node);
    double right_val = parser_eval(node->right_node);


    switch (node->type) {
        case PARSER_NODE_PLUS:   return left_val + right_val;
        case PARSER_NODE_MINUS:  return left_val - right_val;
        case PARSER_NODE_MUL:    return left_val * right_val;
        case PARSER_NODE_DIVIDE: 
            if (right_val == 0.0) {
                C_LOG_ERR("cannot divide 0 by 0!\n");
                return 0.0;
            }
            return left_val / right_val;
        case PARSER_NODE_EQUAL_EQUAL: return (left_val == right_val);
        case PARSER_NODE_BANG_EQUAL: return (left_val != right_val);
        case PARSER_NODE_GREATER_EQUAL: return (left_val >= right_val);
        case PARSER_NODE_LESS_EQUAL: return (left_val <= right_val);
        case PARSER_NODE_GREATER: return (left_val > right_val);
        case PARSER_NODE_LESS: return (left_val < right_val);
        default:
            C_LOG_ERR("[ERROR] parser_eval - UNDEFINED NODE TYPE\n");
            return 0.0;
    }
}

struct parser_t *parser_create_parser(){
    struct parser_t *parser = malloc(sizeof(struct parser_t));
    if(!parser) {C_LOG_ERR("parser_create_parser - couldn't create parser\n"); free(parser); return NULL;}
    parser->node_count = 0;
    parser->nodes = NULL;

    return parser;
}
void parser_delete_parser(struct parser_t **parser){
    if(parser == NULL || *parser == NULL) {C_LOG_ERR("parser_delete_parser - \"struct parser_t **parser\" or \"*parser\" is null\n"); return;}
    free((*parser)->nodes);
    free((*parser));
    *parser = NULL;
}

struct parser_node *parser_create_node(enum parser_node_type type, int line){
    struct parser_node *node = malloc(sizeof(struct parser_node));
    node->type = type;
    node->line = line;

    return node;
}
void parser_delete_node(struct parser_node **node){
    if(node == NULL || (*node) == NULL) {LOG_M_ERR("parser_delete_node - \"struct parser_node *node\" is null\n"); return;}
    if((*node)->left_node) parser_delete_node(&((*node)->left_node));
    if((*node)->right_node) parser_delete_node(&((*node)->right_node));

    free((*node));
    *node = NULL;
}

void parser_parser_add_node(struct parser_t *parser, struct parser_node *node){
    if(node == NULL) {LOG_M_ERR("parser_parser_add_node - \"struct parser_node *node\" is null\n"); return;}
    if(parser == NULL) {LOG_M_ERR("parser_parser_add_node - \"struct parser_t *parser\" is null\n"); return;}

    struct parser_node **tmp = realloc(parser->nodes, sizeof(struct parser_node *) * (parser->node_count+1));
    if(!tmp) {LOG_M_ERR("parser_parser_add_node - couldn't realloc \"parser->nodes\"\n"); return;}
    tmp[parser->node_count] = node;
    parser->nodes = tmp;
    parser->node_count += 1;
    return;
}


static inline struct lexer_token* eat(struct lexer_token *tokens, int token_count, int *cursor, enum token_type expected_type) {
    if (*cursor >= token_count || tokens[*cursor].type != expected_type) {
        C_LOG_ERR("unexpected token type on line %d\n", tokens[*cursor].line);
        return NULL;
    }
    return &tokens[(*cursor)++];
}

int parser_parse(struct parser_t *restrict parser, struct lexer_file *restrict file){
    int cursor = 0;
    for(int statement = 0;statement < file->statement_count; ++statement){

        if(-1 == parser_parse_control_depth(parser, file->tokens, file->token_count, cursor)){
            C_LOG_ERR("expected \"(\" or \")\" on line %d\n", file->tokens[cursor].line);
            return -1;
        }
        struct parser_node *node = parser_parse_boolean_logic(parser, file->tokens, file->token_count, &cursor);
        if(NULL == node) return -1;
        if(NULL == eat(file->tokens, file->token_count, &cursor, LEXER_TOKEN_TYPE_SEMICOLON)) {
            C_LOG_ERR("expected \";\" on line: %d\n", file->tokens[cursor].line);
            return -1;
        }
        parser_parser_add_node(parser, node);

        C_LOG("Result: %f\n", parser_eval(node)); // Debug
    }
    return 0;
}

static inline int is_boolean_logic_token(enum token_type type){ // returns token ID 
    switch (type) {
        case LEXER_TOKEN_TYPE_EQUAL_EQUAL: return 1;
        case LEXER_TOKEN_TYPE_BANG_EQUAL: return 2;

        case LEXER_TOKEN_TYPE_LESS_EQUAL: return 3;
        case LEXER_TOKEN_TYPE_GREATER_EQUAL: return 4;
        case LEXER_TOKEN_TYPE_LESS: return 5;
        case LEXER_TOKEN_TYPE_GREATER: return 6;
        default: return 0;
    }

    return 0; // False
}

struct parser_node *parser_parse_boolean_logic(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(parser == NULL) {LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_t *restrict parser\" is null\n"); return NULL;}
    struct parser_node *left = parser_parse_expression(parser, tokens, token_count, cursor);
    if(left == NULL){
        LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_node *left\" is null\n");
        return NULL;
    }

    int node_type_id = is_boolean_logic_token(tokens[*cursor].type);
    while(*cursor < token_count && node_type_id != 0) {
        enum parser_node_type op_type = node_type_id;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_expression(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_boolean_logic - \"struct parser_node *right\" is null\n");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

        if (*cursor < token_count) {
            node_type_id = is_boolean_logic_token(tokens[*cursor].type);
        } else {
            node_type_id = 0;
        }
    }
    return left;
}

struct parser_node *parser_parse_expression(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(parser == NULL) {LOG_M_ERR("parser_parse_expression - \"struct parser_t *restrict parser\" is null\n"); return NULL;}
    struct parser_node *left = parser_parse_term(parser, tokens, token_count, cursor);
    if(left == NULL){
        LOG_M_ERR("parser_parse_expression - \"struct parser_node *left\" is null\n");
        return NULL;
    }

    while(*cursor < token_count && (tokens[*cursor].type == LEXER_TOKEN_TYPE_PLUS || tokens[*cursor].type == LEXER_TOKEN_TYPE_MINUS)) {
        enum parser_node_type op_type = PARSER_NODE_MINUS;
        if(tokens[*cursor].type == LEXER_TOKEN_TYPE_PLUS) op_type = PARSER_NODE_PLUS;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_term(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_expression - \"struct parser_node *right\" is null\n");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;
    }
    return left;
}
struct parser_node *parser_parse_term(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(parser == NULL) {LOG_M_ERR("parser_parse_term - \"struct parser_t *restrict parser\" is null\n"); return NULL;}

    struct parser_node *left = parser_parse_unary(parser, tokens, token_count, cursor);
    if(left == NULL){
        LOG_M_ERR("parser_parse_term - \"struct parser_node *left\" is null\n");
        return NULL;
    }

    while(*cursor < token_count && (tokens[*cursor].type == LEXER_TOKEN_TYPE_STAR || tokens[*cursor].type == LEXER_TOKEN_TYPE_SLASH)) {
        enum parser_node_type op_type = PARSER_NODE_DIVIDE;
        if(tokens[*cursor].type == LEXER_TOKEN_TYPE_STAR) op_type = PARSER_NODE_MUL;
        eat(tokens, token_count, cursor, tokens[*cursor].type);

        struct parser_node *right = parser_parse_unary(parser, tokens, token_count, cursor);
        if(!right){
            LOG_M_ERR("parser_parse_term - \"struct parser_node *right\" is null\n");
            return NULL;
        }

        struct parser_node *new_node = parser_create_node(op_type, tokens[*cursor].line);
        new_node->left_node = left;
        new_node->right_node = right;
        
        left = new_node;

    }
    return left;
}
struct parser_node *parser_parse_factor(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor){
    if(parser == NULL) {LOG_M_ERR("parser_parse_factor - \"struct parser_t *restrict parser\" is null\n"); return NULL;}
    if(*cursor >= token_count) {
        LOG_M_ERR("parser_parse_factor - \"*cursor >= token_count\"\n");
        return NULL;
    }

    if(tokens[*cursor].type == LEXER_TOKEN_TYPE_INT_LITERAL) {
        struct parser_node *node = parser_create_node(PARSER_NODE_NUMBER, tokens[*cursor].line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_INT_LITERAL);

        if(t){
            node->value = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_IDENTIFIER){
        struct parser_node *node = parser_create_node(PARSER_NODE_IDENTIFIER, tokens[*cursor].line);
        struct lexer_token *t = eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_IDENTIFIER);

        if(t){
            node->value = t->token;
        }else {
            parser_delete_node(&node);
            return NULL;
        }
        return node;
    }else if(tokens[*cursor].type == LEXER_TOKEN_TYPE_LPAREN){
        eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_LPAREN);

        struct parser_node *node = parser_parse_boolean_logic(parser, tokens, token_count, cursor);
        if(!eat(tokens, token_count, cursor, LEXER_TOKEN_TYPE_RPAREN)){
            C_LOG_ERR("expected \")\" on line: %d\n", tokens[*cursor].line);
            parser_delete_node(&node);
            (*cursor)+=1;
            return NULL;
        }

        return node;
    }else {
        C_LOG_ERR("parser_parse_factor - current token is not literal or identifier (unexpected token), line: %d\n", tokens[*cursor].line);
        return NULL;
    }
    return NULL;
}

struct parser_node *parser_parse_unary(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int *cursor) {
    if(*cursor >= token_count) {LOG_M_ERR("parser_parse_unary - \"*cursor >= token_count\"\n"); return NULL;}
    enum token_type tok_type = tokens[*cursor].type;
    if (tok_type == LEXER_TOKEN_TYPE_MINUS || tok_type == LEXER_TOKEN_TYPE_PLUS || tok_type == LEXER_TOKEN_TYPE_BANG) {
        int op_line = tokens[*cursor].line;

        struct lexer_token *op_token = &tokens[*cursor];
        (*cursor)++;

        struct parser_node *right_node = parser_parse_unary(parser, tokens, token_count, cursor);

        if (op_token->type == LEXER_TOKEN_TYPE_PLUS) {
            return right_node; 
        }

        struct parser_node *node = parser_create_node((tok_type == LEXER_TOKEN_TYPE_MINUS) ? PARSER_NODE_UNARY_MINUS : PARSER_NODE_UNARY_BANG, op_line);

        node->right_node = right_node;
        
        return node;
    }

    return parser_parse_factor(parser, tokens, token_count, cursor);
}

int parser_parse_control_depth(struct parser_t *restrict parser, struct lexer_token *restrict tokens, int token_count, int cursor){
    int depth = 0;
    int max_depth = 0;
    while(cursor < token_count){
        // if(tokens[cursor].type == LEXER_TOKEN_TYPE_SEMICOLON) {
        //     cursor++;
        //
        //     break;
        // }
        enum token_type current_type = tokens[cursor].type;
        if(current_type == LEXER_TOKEN_TYPE_LPAREN) {depth++;}
        else if(current_type == LEXER_TOKEN_TYPE_RPAREN) {depth--;}

        if(depth < 0) return -1;

        cursor++;
    }
    return 0;
}
