#include "core/semantic_analyzer.h"
#include "core/flags/function_flags.h"
#include "core/globals.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stdio.h>

static void propagate_literal_types(struct parser_node *node, struct type_info *target_type) {
    if (!node || !target_type) return;

    if (node->type == PARSER_NODE_NUMBER) {
        if (1 == type_table_can_that_promote_to(node->type_info, target_type)) {
            node->type_info = target_type;
        }
        return;
    }

    if (node->type == PARSER_NODE_IDENTIFIER) {
        return;
    }

    if (1 == type_table_can_that_promote_to(node->type_info, target_type)) {
        node->type_info = target_type;
    }

    if (node->left_node) propagate_literal_types(node->left_node, target_type);
    if (node->right_node) propagate_literal_types(node->right_node, target_type);
}

static inline void print_semantic_error_type_infos(struct parser_node *node) {
    C_LOG_ERR("Semantic: cannot use those types in same instruction, on line %d", node->line);
}

static inline void print_semantic_error_call_calling_function_not_found(struct parser_node *node, struct str_view calling_function_name) {
    C_LOG_ERR("Semantic: calling function (" SV_FMT ") is not found in the scope, on line %d", SV_ARG(calling_function_name) ,node->line);
}


static inline void print_semantic_error_call_arguments_are_not_matching(struct parser_node *node, struct str_view calling_function_name) {
    C_LOG_ERR("Semantic: calling function's (" SV_FMT ") arguments are not matching with the call, on line %d", SV_ARG(calling_function_name) ,node->line);
}


static inline void print_semantic_error_pure_func(struct parser_node *node) {
    C_LOG_ERR("Semantic: pure function cannot manipulate pointers and can only call other pure functions, on line %d", node->line);
}

static int is_statement_pure(struct parser_node *node, struct semantic_context *context) { // if it is pure, returns 1

    int is_pure = 1;

    switch (node->type) {
        case PARSER_NODE_ASM: return 0;
        case PARSER_NODE_VARIABLE_ASSIGMENT: 
                              if(PARSER_NODE_UNARY_DEREFERENCE == node->left_node->type || TYPE_CATEGORY_POINTER == node->left_node->type_info->category) {
                                  print_semantic_error_pure_func(node);
                                  context->error = 1;
                                  return 0;
                              }
                              return 1;

        case PARSER_NODE_CALL:{
                              struct str_view calling_function_name = node->data.call.name;
                              struct symbol_t *sym = symbol_table_look_up(context->current_scope, calling_function_name);
                              if(NULL == sym) {
                                  print_semantic_error_call_calling_function_not_found(node, calling_function_name);
                                  context->error = 1;
                                  return 0;
                              }
                              if(!function_flags_get_is_pure_function(sym->flags)) {
                                  print_semantic_error_pure_func(node);
                                  context->error = 1;
                                  return 0;
                              }
                              }

                              return 1;
        default: return 1;
    }

    if (!is_statement_pure(node->left_node, context))  return 1;
    if (!is_statement_pure(node->right_node, context)) return 1;

    return is_pure;
}

int semantic_analyzer_run_analyzer(struct parser_t *parser) {
    struct semantic_context context;
    context.type_table = parser->type_table;
    context.error = 0;

    int node_count = parser->nodes->element_count;
    for(int i = 0; i < node_count; ++i) {
        context.current_block = NULL;
        context.current_function = NULL;
        context.current_scope = parser->current_scope;

        struct parser_node *node = *(struct parser_node **) vector_get(parser->nodes, i);
        semantic_analyzer_calculate_type_infos(node, &context);


        context.current_block = NULL;
        context.current_function = NULL;
        context.current_scope = parser->current_scope;

        semantic_analyzer_analyze_node(node, &context);
    }
    return context.error;
}

int semantic_analyzer_analyze_node(struct parser_node* node, struct semantic_context* context) {
    if(node->left_node) semantic_analyzer_analyze_node(node->left_node, context);
    if(node->right_node) semantic_analyzer_analyze_node(node->right_node, context);

    switch (node->type) {
        case PARSER_NODE_FUNCTION: return semantic_analyzer_analyze_function_decl(node, context);
        case PARSER_NODE_MODULE:{
            int function_count = node->data.module.functions->element_count;
            for(int i = 0; i < function_count; ++i){
                struct parser_node *curr = *(struct parser_node **) vector_get(node->data.module.functions, i);
                semantic_analyzer_analyze_node(curr, context);
            }
            break;
        }
        case PARSER_NODE_BLOCK: return semantic_analyzer_analyze_block(node, context);
        case PARSER_NODE_VARIABLE_DECLARATION: return semantic_analyzer_analyze_var_declaration(node, context);
        case PARSER_NODE_VARIABLE_ASSIGMENT: return semantic_analyzer_analyze_assigment(node, context);
        case PARSER_NODE_CALL: return semantic_analyzer_analyze_call(node, context);

        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_GREATER_EQUAL:
        case PARSER_NODE_LESS:
        case PARSER_NODE_GREATER: return semantic_analyzer_analyze_binary_expr(node, context);

        default:
        return 0;
    }
    return 1;
}

int semantic_analyzer_analyze_block(struct parser_node *node, struct semantic_context *context) {
    struct symbol_table *old_scope = context->current_scope;
    context->current_block = node;
    context->current_scope = node->data.block.scope;

    int err = 0;
    int statement_count = node->data.block.count;
    for(int i = 0;i < statement_count; ++i) {
        struct parser_node *curr = *(struct parser_node **) vector_get(node->data.block.statements, i);
        err |= semantic_analyzer_analyze_node(curr, context);
    }
    context->current_scope = old_scope;
    return err;
}

int semantic_analyzer_analyze_function_decl(struct parser_node* node, struct semantic_context* context) {
    struct symbol_table *old_scope = context->current_scope;

    context->current_function = node;
    int statement_count = node->data.function.body->data.block.count;
    int err = 0;
    err |= semantic_analyzer_analyze_node(node->data.function.body, context);

    if(function_flags_get_is_pure_function(node->data.function.flags)) {
        int is_pure_function = 1;
        for(int i = 0; i < statement_count; ++i) {
            struct parser_node *statement = *(struct parser_node **) vector_get(node->data.function.body->data.block.statements, i);
            int is_pure = is_statement_pure(statement, context);
            is_pure_function &= is_pure;
        }
        if(0 == is_pure_function) {
            err = 1;
            C_LOG_ERR("Semantic: function is not pure, on line %d", node->line);
        }

    }

    context->current_scope = old_scope;
    return err;
}
int semantic_analyzer_analyze_var_declaration(struct parser_node* node, struct semantic_context* context) {
    struct symbol_t *sym = symbol_table_look_up(context->current_scope, node->data.variable.variable_name);
    node->data.variable.symbol = sym;
    if(!sym) {
        context->error = 1;
        return 1;
    }

    if(1 != type_table_can_that_promote_to(node->right_node->type_info, sym->type)) {
        print_semantic_error_type_infos(node);
        context->error = 1;
    }
    return 0;
}
int semantic_analyzer_analyze_assigment(struct parser_node* node, struct semantic_context* context) {
    if(1 != type_table_can_that_promote_to(node->right_node->type_info, node->left_node->type_info)) {
        print_semantic_error_type_infos(node);
        context->error = 1;
    }
    return 0;
}
int semantic_analyzer_analyze_call(struct parser_node* node, struct semantic_context* context) {
    struct str_view calling_function_name = node->data.call.name;
    struct symbol_t *sym = symbol_table_look_up(context->current_scope, calling_function_name);
    if(NULL == sym) {
        print_semantic_error_call_calling_function_not_found(node, calling_function_name);
        context->error = 1;
        return 1;
    }

    struct parser_node *parameters = sym->function.parameters;
    int param_count = parameters->data.block.count;

    if(param_count != node->data.call.arg_count) {
        print_semantic_error_call_arguments_are_not_matching(node, calling_function_name);
        context->error = 1;
        return 1;
    }

    int err = 0;

    for(int i = 0;i < param_count; ++i) {
        struct parser_node *arg_node = *(struct parser_node **) vector_get(node->data.call.args, i);
        struct parser_node *param_node = *(struct parser_node **) vector_get(parameters->data.block.statements, i);
        struct type_info *arg_type = arg_node->type_info;
        struct type_info *param_type = param_node->type_info;

        if(NULL == arg_type) arg_type = get_literals_type_info(context->type_table, param_type, arg_node->type);
        if(NULL == arg_type) {
            print_semantic_error_call_arguments_are_not_matching(node, calling_function_name);
            context->error = 1;
            err = 1;
        }else {
            if(arg_type->type_id != param_type->type_id) {
                if (arg_type->type_id == param_type->type_id) {
                    node->type_info = arg_type;
                }else if (1 == type_table_can_that_promote_to(arg_type, param_type)) {
                    arg_type = param_type; 
                }else {
                    print_semantic_error_call_arguments_are_not_matching(node, calling_function_name);
                    context->error = 1;
                    node->type_info = NULL;
                }
            }

        }
    }

    return err;
}
int semantic_analyzer_analyze_binary_expr(struct parser_node* node, struct semantic_context* context) {
    return 0;
}

struct type_info *semantic_analyzer_calculate_type_infos(struct parser_node *node, struct semantic_context *context) {
    if(NULL == node) return NULL;
    struct type_info *left_type = NULL;
    struct type_info *right_type = NULL;
    if(node->left_node) left_type = semantic_analyzer_calculate_type_infos(node->left_node, context);
    if(node->right_node) right_type = semantic_analyzer_calculate_type_infos(node->right_node, context);

    switch (node->type) {
        case PARSER_NODE_FUNCTION:{
            context->current_function = node;
            struct type_info *info = semantic_analyzer_calculate_type_infos(node->data.function.body, context);
            if(1 == type_table_can_that_promote_to(info, node->data.function.return_type)) {
                node->type_info = node->data.function.return_type;
                break;
            }
            print_semantic_error_type_infos(node);
            context->error = 1;
            break;
        }case PARSER_NODE_MODULE:{
            int function_count = node->data.module.functions->element_count;
            for(int i = 0; i < function_count; ++i){
                struct parser_node *curr = *(struct parser_node **) vector_get(node->data.module.functions, i);
                semantic_analyzer_calculate_type_infos(curr, context);
            }
            break;
        }case PARSER_NODE_BLOCK: {
            struct symbol_table *old_scope = context->current_scope;

            context->current_block = node;
            context->current_scope = node->data.block.scope;
            int statement_count = node->data.block.count;
            for(int i = 0; i < statement_count; ++i){
                struct parser_node *statement = *(struct parser_node **) vector_get(node->data.block.statements, i);
                node->type_info = semantic_analyzer_calculate_type_infos(statement, context);
            }

            context->current_scope = old_scope;
            break;
        }case PARSER_NODE_CALL: {
            int arg_count = node->data.call.arg_count;

            for(int i = 0;i < arg_count; ++i) {
                struct parser_node *arg_node = *(struct parser_node **) vector_get(node->data.call.args, i);
                semantic_analyzer_calculate_type_infos(arg_node, context);
            }
            struct symbol_t *fnc_sym = symbol_table_look_up(context->current_scope, node->data.call.name);
            if(!fnc_sym) {
                print_semantic_error_call_calling_function_not_found(node, node->data.call.name);
                context->error = 1;
                return NULL;
            }
            node->type_info = fnc_sym->function.return_type;
            break;
        }case PARSER_NODE_IDENTIFIER: {
            struct symbol_t *sym = symbol_table_look_up(context->current_scope, node->data.variable.variable_name);
            if(sym) {
                node->data.variable.symbol = sym;
                node->type_info = sym->type;
            }else {
                node->type_info = NULL;
            }
            break;
        }case PARSER_NODE_ASM: {
            node->type_info = NULL;
            break;
        }case PARSER_NODE_LOOP: {
            semantic_analyzer_calculate_type_infos(node->data.loop.body_block, context);
            semantic_analyzer_calculate_type_infos(node->data.loop.continue_block, context);
            node->type_info = semantic_analyzer_calculate_type_infos(node->data.loop.return_block, context);
            break;
        }case PARSER_NODE_UNARY_BANG: {
            if(TYPE_CATEGORY_POINTER != right_type->category && TYPE_CATEGORY_BASIC != right_type->category) {
                print_semantic_error_type_infos(node);
                node->type_info = NULL;
                context->error = 1;
                break;
            }
            node->type_info = type_table_get_type_info_cstr(context->type_table, "bool", 0);
            break;
        }case PARSER_NODE_UNARY_MINUS: {
            if(TYPE_CATEGORY_BASIC != right_type->category) {
                print_semantic_error_type_infos(node);
                node->type_info = NULL;
                context->error = 1;
                break;
            }
            node->type_info = right_type;
            break;
        }case PARSER_NODE_UNARY_DEREFERENCE: {
            if (node->right_node && node->right_node->type_info && node->right_node->type_info->pointer_level > 0 && node->right_node->type_info->points_to) {

                node->type_info = node->right_node->type_info->points_to;
            } else {
                print_semantic_error_type_infos(node);
                context->error = 1;
                node->type_info = NULL;
            }
            break;
        }case PARSER_NODE_UNARY_ADDRESS_OF: {
            int pointer_level = node->right_node->type_info->pointer_level;
            
            struct type_info *target = type_table_get_or_create_pointer_type_info(context->type_table, node->right_node->type_info->name, pointer_level+1);
            node->type_info = target;

            node->right_node->data.variable.symbol->is_address_taken = true;
            node->right_node->data.variable.symbol->location_kind = LOCATION_STACK;
            break;
        }case PARSER_NODE_VARIABLE_DECLARATION: {
            struct symbol_t *sym = symbol_table_look_up(context->current_scope, node->data.variable.variable_name);
            node->data.variable.symbol = sym;
            if(sym) {
                node->type_info = sym->type;
                propagate_literal_types(node->right_node, node->type_info);
            }else {
                node->type_info = NULL;
            }
            break;
        }case PARSER_NODE_VARIABLE_ASSIGMENT: {
            if (node->left_node && node->left_node->type_info) {
                node->type_info = node->left_node->type_info;
            } else {
                node->type_info = NULL;
            }

            if (node->left_node && node->left_node->type == PARSER_NODE_IDENTIFIER) {
                struct symbol_t *sym = symbol_table_look_up(context->current_scope, node->left_node->data.variable.variable_name);
                node->data.variable.symbol = sym;
            }

            if (node->type_info) {
                propagate_literal_types(node->right_node, node->type_info);
            }
            break;
        }case PARSER_NODE_NUMBER: {
            node->type_info = get_literals_type_info(context->type_table, NULL, node->type);
            break;
        }case PARSER_NODE_STRING: {
            node->type_info = get_literals_type_info(context->type_table, NULL, node->type);
            break;
        }
        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_GREATER_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_LESS:
        case PARSER_NODE_GREATER:
            node->type_info = type_table_get_type_info_cstr(context->type_table, "bool", 0);

            if(left_type && right_type) {
                if (1 == type_table_can_that_promote_to(left_type, right_type)) {
                    left_type = right_type; 
                } 
                else if (1 == type_table_can_that_promote_to(right_type, left_type)) {
                    right_type = left_type;
                }else {
                    print_semantic_error_type_infos(node);
                    context->error = 1;
                    node->type_info = get_literals_type_info(context->type_table, NULL, node->type);
                }
            }

            break;
        default: {
            if(left_type && right_type) {
                if (node->type == PARSER_NODE_PLUS || node->type == PARSER_NODE_MINUS) {
                    if (left_type->category == TYPE_CATEGORY_POINTER && right_type->category == TYPE_CATEGORY_BASIC) {
                        node->right_node->type_info = context->type_table->pointer_to_int_type;
                        

                        node->type_info = left_type;
                        break;
                    }

                    if (node->type == PARSER_NODE_PLUS && 
                            left_type->category == TYPE_CATEGORY_BASIC && 
                            right_type->category == TYPE_CATEGORY_POINTER) {

                        node->left_node->type_info = context->type_table->pointer_to_int_type;

                        node->type_info = right_type;
                        break;
                    }

                    if (node->type == PARSER_NODE_MINUS && 
                            left_type->category == TYPE_CATEGORY_POINTER && 
                            right_type->category == TYPE_CATEGORY_POINTER) {
                        node->type_info = context->type_table->pointer_to_int_type;
                        break;
                    }
                }
                if (left_type->type_id == right_type->type_id) {
                    node->type_info = left_type;
                }else if (1 == type_table_can_that_promote_to(left_type, right_type)) {
                    node->type_info = right_type; 
                }else if (1 == type_table_can_that_promote_to(right_type, left_type)) {
                    node->type_info = left_type;
                }else {
                    print_semantic_error_type_infos(node);
                    context->error = 1;
                    node->type_info = NULL;
                    break;
                }
            }else if(left_type) {
                node->type_info = left_type;
            }else if(right_type) {
                node->type_info = right_type;
            }
            break;
        }

    }
    return node->type_info;
}
