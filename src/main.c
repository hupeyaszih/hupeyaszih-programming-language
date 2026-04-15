#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void lexer_test(struct parser_t *restrict parser, char fl[]){
    printf("\n\n\nFile: %s\n", fl);

    struct lexer_file *file = malloc(sizeof(struct lexer_file));
    lexer_create_lexer_file(file, fl);
    
    for(int i = 0;i < file->token_count; ++i){
        printf("%s | token type: %s\n", (*(file->tokens+i)).token, lexer_token_type_to_string((*(file->tokens+i)).type));
    }
    
    printf("Total token count: %d\n", file->token_count);

    int res = parser_parse(parser, file);

    lexer_delete_lexer_file(file);
}

void parser_print_tree(struct parser_node *node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) printf("  ");

    printf("|- Node [Start: %d, Type: %s ,End: %d, Subs: %d]\n", 
            node->start_index, parser_node_type_to_string(node->type),node->end_index, node->sub_node_count);

    // for (int i = node->start_index; i < node->end_index; ++i) {
    //     if(node->start_index == -1 || i < 0 || i >= node->file->token_count || node->end_index == -1) continue;
    //     printf("%s", node->file->tokens[i].token);
    // }
    // printf("\n");

    for (int i = 0; i < node->sub_node_count; i++) {
        parser_print_tree(node->sub_nodes[i], depth + 1);
    }
}

int main() {
    struct parser_t *parser;
    parser_create_parser(&parser, 64, 64);

    lexer_test(parser, "var year:int32 = ((550+150)-2026);");
    lexer_test(parser, "(10*(90+(10)));");

    
    for(int i = 0; i < parser->node_count; ++i){
       struct parser_node *main_node = *(parser->nodes+i);
       parser_print_tree(main_node, 5);
    }

    parser_delete_parser(parser);
    return 0;
}
