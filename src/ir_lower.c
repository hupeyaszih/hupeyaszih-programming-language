#include "ir_lower.h"
#include "globals.h"
#include "ir_gen.h"
#include "parser.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline struct IR_Instruction *create_jmp_inst(struct IR_Block *block_to_jump) {
    struct IR_Instruction *inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_JMP);
    inst->data.target_block = block_to_jump;
    return inst;
}

static inline char *create_mangled_function_name(const struct IR_Builder *builder, char *original_name, int scope_id) {
    if (builder->current_function) {
        int required_len = snprintf(NULL, 0, "%s_S_%d", original_name, scope_id);

        char *mangled_name = malloc(required_len + 1);

        if (mangled_name) {
            sprintf(mangled_name, "%s_S_%d", original_name, scope_id);
            return mangled_name;
        }
    }

    return strdup(original_name);
}

static inline char *create_mangled_block_name(const struct IR_Builder *restrict builder, char *name) {
    if(NULL == builder->current_scope) {
        int required_len = snprintf(NULL, 0, "%s_%s_S", builder->current_function->name, name);
        char *mangled_name = malloc(required_len + 1);

        if (mangled_name) {
            sprintf(mangled_name, "%s_%s_S", builder->current_function->name, name);
            return mangled_name;
        }

        return NULL;
    }

    int required_len = snprintf(NULL, 0, "%s_%s_S_%d", builder->current_function->name, name, builder->current_scope->scope_id);
    char *mangled_name = malloc(required_len + 1);

    if (mangled_name) {
        sprintf(mangled_name, "%s_%s_S_%d", builder->current_function->name, name, builder->current_scope->scope_id);
        return mangled_name;
    }

    return NULL;
}

struct IR_Builder *IRL_create_IR_Builder() {
    struct IR_Builder *builder = calloc(1, sizeof(struct IR_Builder));

    return builder;
}
int IRL_delete_IR_Builder(struct IR_Builder **builder){
    if(NULL == builder || NULL == *builder) return 1;
    free((*builder));
    *builder = NULL;
    return 0;
}


struct IR_Module* IRLower_program(struct IR_Builder *restrict builder, struct parser_t *restrict parser){
    struct IR_Module *module = IR_create_IR_Module();
    builder->module = module;

    for(int i = 0; i < parser->node_count; ++i){
        if(parser->nodes[i]->type == PARSER_NODE_FUNCTION) {
            IRLower_function(builder, parser->nodes[i]);
        }
    }


    return module;
}

struct IR_Operand *IRLower_block(struct IR_Builder *builder, struct parser_node *node) {
    if (!node) return NULL;
    struct symbol_table *prev_table = builder->current_scope;

    if(PARSER_NODE_BLOCK == node->type) {
        builder->current_scope = node->data.block.scope;
    }


    struct IR_Operand *last_operand = NULL;
    int index = 0;
    struct parser_node *curr = node->data.block.statements[index++];
    while(curr) {
        if(index > node->data.block.count) break;
        last_operand = IRLower_statement(builder, curr);
        curr = node->data.block.statements[index++];
    }
    builder->current_scope = prev_table;
    return last_operand;
}

struct IR_Operand *IRLower_statement(struct IR_Builder *builder, struct parser_node *node){
    if (!node) return NULL;
    switch (node->type) {
        case PARSER_NODE_BLOCK:
            return IRLower_block(builder, node);
            break;
        case PARSER_NODE_VARIABLE_DECLARATION:{
            const char *var_name = node->data.variable_name;
            struct symbol_t *sym = symbol_table_look_up(builder->current_scope, var_name);

            struct IR_Operand *addr_reg = IRL_new_vreg(builder);
            struct IR_Instruction *alloca_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_ALLOCA);
            alloca_inst->destination = addr_reg;
            IR_Block_add_instruction(builder->current_block, alloca_inst);

            sym->ir_operand = addr_reg; 

            if (node->right_node) {
                struct IR_Operand *val = IRLower_expression(builder, node->right_node);
                struct IR_Instruction *store_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_STORE);
                store_inst->source_1 = val;
                store_inst->destination = addr_reg; 
                IR_Block_add_instruction(builder->current_block, store_inst);
                return val;
            }
            break;
        }case PARSER_NODE_VARIABLE_ASSIGMENT: {
            struct IR_Operand *source = IRLower_expression(builder, node->right_node);

            struct IR_Operand *dest_addr = IRLower_address(builder, node->left_node);

            if (dest_addr) {
                if (source->type == IR_OPERAND_TYPE_IMM) {
                    struct IR_Operand *reg = IRL_new_vreg(builder);
                    struct IR_Instruction *copy = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_COPY);
                    copy->destination = reg;
                    copy->source_1 = source;
                    IR_Block_add_instruction(builder->current_block, copy);
                    source = reg;
                }

                struct IR_Instruction *store_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_STORE);
                store_inst->destination = dest_addr;
                store_inst->source_1 = source;
                IR_Block_add_instruction(builder->current_block, store_inst);
                return store_inst->source_1;
            }
            break;
        }case PARSER_NODE_LOOP:{
            IRLower_loop(builder, node);
            break;
        }case PARSER_NODE_BREAK:{
            IRLower_break(builder, node);
            break;
        }case PARSER_NODE_CONTINUE:{
            IRLower_continue(builder, node);
            break;
        }case PARSER_NODE_FUNCTION: {
            IRLower_function(builder, node);
            break;
        }case PARSER_NODE_ASM: {
            struct IR_Instruction *inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_ASM);
            
            inst->data.asm_code = strdup(node->data.assembly.assembly_data);
            
            IR_Block_add_instruction(builder->current_block, inst);
            break;
        }
        case PARSER_NODE_CALL:
        case PARSER_NODE_IDENTIFIER:
        case PARSER_NODE_NUMBER:
        case PARSER_NODE_PLUS:
        case PARSER_NODE_MINUS:
        case PARSER_NODE_MUL:
        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_GREATER_EQUAL:
        case PARSER_NODE_GREATER:
        case PARSER_NODE_LESS:
        case PARSER_NODE_DIVIDE:
        case PARSER_NODE_UNARY_ADDRESS_OF:
        case PARSER_NODE_UNARY_BANG:
        case PARSER_NODE_UNARY_DEREFERENCE:
        case PARSER_NODE_UNARY_MINUS:
            return IRLower_expression(builder, node);

        default:
            break;
    }

    return NULL;
}
struct IR_Operand* IRLower_address(struct IR_Builder *builder, struct parser_node *node) {
    switch (node->type) {
        case PARSER_NODE_IDENTIFIER: {
            struct symbol_t *sym = symbol_table_look_up(builder->current_scope, node->data.variable_name);
            return sym->ir_operand; 
        }
        case PARSER_NODE_UNARY_DEREFERENCE: {
            return IRLower_expression(builder, node->right_node);
        }
        default:
            C_LOG_ERR("L-value expected");
            return NULL;
    }
}
void IRLower_function(struct IR_Builder *builder, struct parser_node *node) {
    struct IR_Function *prev_func = builder->current_function;
    struct IR_Block *prev_block = builder->current_block;
    struct symbol_table *prev_scope = builder->current_scope;

    struct IR_Function *func = IR_create_IR_Function();
    func->parameter_count = node->data.function.param_count;
    func->name = strdup(node->data.function.name);
    if(NULL != builder->current_scope) {
        func->label = create_mangled_function_name(builder, node->data.function.name, builder->current_scope->scope_id);
    }else {
        func->label = create_mangled_function_name(builder, node->data.function.name, 0);
    }

    struct symbol_t *func_sym = symbol_table_look_up(builder->current_scope, node->data.function.name);
    if (func_sym) {
        if (func_sym->mangled_name) {
            free(func_sym->mangled_name);
        }
        func_sym->mangled_name = strdup(func->label);
    }
    
    IR_Module_add_function(builder->module, func);
    builder->current_function = func;

    struct IR_Block *entry = IR_create_IR_Block();
    entry->label = strdup("entry");
    IR_Function_add_block(func, entry);
    builder->current_block = entry;

    builder->current_function->vg_counter = node->data.function.param_count;
    builder->current_scope = node->data.function.body->data.block.scope;
    for (int i = 0; i < node->data.function.param_count; i++) {
        struct IR_Operand *param_val = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, builder->current_function);
        param_val->value.vreg_id = i; 

        struct IR_Operand *addr = IRL_new_vreg(builder);

        struct IR_Instruction *alloca_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_ALLOCA);
        alloca_inst->destination = addr;
        IR_Block_add_instruction(builder->current_block, alloca_inst);

        struct IR_Instruction *store_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_STORE);
        store_inst->source_1 = param_val;
        store_inst->destination = addr;
        IR_Block_add_instruction(builder->current_block, store_inst);

        struct parser_node *param_node = node->data.function.params->data.block.statements[i];
        struct symbol_t *sym = symbol_table_look_up(builder->current_scope, param_node->data.variable_name);
        if (sym) {
            sym->ir_operand = addr;
        }
    }

    struct IR_Operand *last_operand = IRLower_block(builder, node->data.function.body);
    struct IR_Instruction *ret_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_RET);
    ret_inst->source_1 = last_operand;
    
    IR_Block_add_instruction(builder->current_block, ret_inst);

    builder->current_function = prev_func;
    builder->current_block = prev_block;
    builder->current_scope = prev_scope;
}

void IRLower_loop(struct IR_Builder *builder, struct parser_node *node) {
    struct IR_Block *header_bb = IR_create_IR_Block();
    header_bb->label = create_mangled_block_name(builder, "loop_header");
    
    struct IR_Block *body_bb = IR_create_IR_Block();
    body_bb->label = create_mangled_block_name(builder, "loop_body");

    struct IR_Block *exit_bb = IR_create_IR_Block();
    exit_bb->label = create_mangled_block_name(builder, "loop_exit");

    IR_Block_add_instruction(builder->current_block, create_jmp_inst(header_bb));
    
    IR_Function_add_block(builder->current_function, header_bb);
    IR_Block_add_instruction(header_bb, create_jmp_inst(body_bb));

    IR_Function_add_block(builder->current_function, body_bb);
    builder->current_block = body_bb;

    struct IR_Block *prev_header = builder->current_loop_header;
    struct IR_Block *prev_exit = builder->current_loop_exit;
    builder->current_loop_header = header_bb;
    builder->current_loop_exit = exit_bb;

    IRLower_block(builder, node->data.loop.body);

    IR_Block_add_instruction(builder->current_block, create_jmp_inst(header_bb));

    IR_Function_add_block(builder->current_function, exit_bb);
    builder->current_block = exit_bb;

    builder->current_loop_header = prev_header;
    builder->current_loop_exit = prev_exit;
}

void IRLower_break(struct IR_Builder *builder, struct parser_node *node) {
    if (builder->current_loop_exit == NULL) {
        C_LOG_ERR("Cannot use \"break\" outside of a loop");
        return; 
    }
    struct IR_Operand *cond = IRLower_expression(builder, node->data.loop_control.condition);

    struct IR_Block *break_body_bb = IR_create_IR_Block();
    break_body_bb->label = create_mangled_block_name(builder, "break_body");
    
    struct IR_Block *next_bb = IR_create_IR_Block();
    next_bb->label = create_mangled_block_name(builder, "after_break_check");

    struct IR_Instruction *br = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_BR_COND);
    br->source_1 = cond;
    br->data.br_target.true_block = break_body_bb;
    br->data.br_target.false_block = next_bb;
    IR_Block_add_instruction(builder->current_block, br);

    IR_Function_add_block(builder->current_function, break_body_bb);
    builder->current_block = break_body_bb;
    
    IRLower_block(builder, node->data.loop_control.body); 
    
    struct IR_Instruction *exit_jmp = create_jmp_inst(builder->current_loop_exit);
    IR_Block_add_instruction(builder->current_block, exit_jmp);

    IR_Function_add_block(builder->current_function, next_bb);
    builder->current_block = next_bb;
}

void IRLower_continue(struct IR_Builder *builder, struct parser_node *node) {
    if (builder->current_loop_exit == NULL) {
        C_LOG_ERR("Cannot use \"continue\" outside of a loop");
        return; 
    }
    struct IR_Operand *cond = IRLower_expression(builder, node->data.loop_control.condition);

    struct IR_Block *continue_body_bb = IR_create_IR_Block();
    continue_body_bb->label = create_mangled_block_name(builder, "continue_body");
    
    struct IR_Block *next_bb = IR_create_IR_Block();
    next_bb->label = create_mangled_block_name(builder, "after_continue_check");

    struct IR_Instruction *br = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_BR_COND);
    br->source_1 = cond;
    br->data.br_target.true_block = continue_body_bb;
    br->data.br_target.false_block = next_bb;
    IR_Block_add_instruction(builder->current_block, br);

    IR_Function_add_block(builder->current_function, continue_body_bb);
    builder->current_block = continue_body_bb;
    
    IRLower_block(builder, node->data.loop_control.body); 
    
    struct IR_Instruction *header_jmp = create_jmp_inst(builder->current_loop_header);
    IR_Block_add_instruction(builder->current_block, header_jmp);

    IR_Function_add_block(builder->current_function, next_bb);
    builder->current_block = next_bb;
}

struct IR_Operand* IRLower_expression(struct IR_Builder *builder, struct parser_node *node) {
    if (!node) return NULL;

    switch (node->type) {
        case PARSER_NODE_NUMBER: {
            struct IR_Operand *imm = IR_create_IR_Operand(IR_OPERAND_TYPE_IMM, builder->current_function);
            imm->value.imm = strdup(node->data.literal_data);
            return imm;
        }case PARSER_NODE_IDENTIFIER: {
            struct symbol_t *sym = symbol_table_look_up(builder->current_scope, node->data.variable_name);

            struct IR_Operand *val_reg = IRL_new_vreg(builder);
            struct IR_Instruction *load_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_LOAD);
            load_inst->destination = val_reg;
            load_inst->source_1 = sym->ir_operand; 
            IR_Block_add_instruction(builder->current_block, load_inst);

            return val_reg; 
        }case PARSER_NODE_CALL: {
            struct IR_Instruction *call_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_CALL);

            struct symbol_t *func_sym = symbol_table_look_up(builder->current_scope, node->data.call.name);
            if (func_sym && func_sym->mangled_name) {
                call_inst->data.call_info.function_label = strdup(func_sym->mangled_name);
            } else {
                call_inst->data.call_info.function_label = strdup(node->data.call.name);
            }

            for (int i = 0; i < node->data.call.arg_count; i++) {
                struct parser_node *arg_node = node->data.call.args[i];
                struct IR_Operand *arg_op = IRLower_expression(builder, arg_node);

                vector_add(call_inst->data.call_info.arguments, &arg_op);
            }

            struct IR_Operand *ret_vreg = IRL_new_vreg(builder);
            call_inst->destination = ret_vreg;

            IR_Block_add_instruction(builder->current_block, call_inst);

            return ret_vreg;
        }
        case PARSER_NODE_PLUS:
        case PARSER_NODE_MINUS:
        case PARSER_NODE_MUL:
        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_GREATER_EQUAL:
        case PARSER_NODE_GREATER:
        case PARSER_NODE_LESS:
        case PARSER_NODE_DIVIDE:
            return IRLower_binary_op(builder, node);
        case PARSER_NODE_UNARY_BANG:
        case PARSER_NODE_UNARY_MINUS:
            return IRLower_unary_op(builder, node);
        case PARSER_NODE_UNARY_ADDRESS_OF:
            return IRLower_address(builder, node->right_node);
        case PARSER_NODE_UNARY_DEREFERENCE: {
            struct IR_Operand *addr = IRLower_expression(builder, node->right_node);
            struct IR_Operand *val_reg = IRL_new_vreg(builder);

            struct IR_Instruction *load_inst = IR_create_IR_Instruction(IR_INSTRUCTION_TYPE_LOAD);
            load_inst->destination = val_reg;
            load_inst->source_1 = addr;
            IR_Block_add_instruction(builder->current_block, load_inst);

            return val_reg;
        }
    }
    return NULL;
}
struct IR_Operand* IRLower_binary_op(struct IR_Builder *builder, struct parser_node *node) {
    struct IR_Operand *left = IRLower_expression(builder, node->left_node);
    struct IR_Operand *right = IRLower_expression(builder, node->right_node);

    struct IR_Operand *dest = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, builder->current_function);
    dest->value.vreg_id = builder->current_function->vg_counter++;

    enum IR_InstructionType type;
    if (node->type == PARSER_NODE_PLUS) type = IR_INSTRUCTION_TYPE_ADD;
    else if (node->type == PARSER_NODE_MINUS) type = IR_INSTRUCTION_TYPE_SUB;
    else if (node->type == PARSER_NODE_DIVIDE) type = IR_INSTRUCTION_TYPE_DIV;
    else if (node->type == PARSER_NODE_EQUAL_EQUAL) type = IR_INSTRUCTION_TYPE_EQUAL_EQUAL;
    else if (node->type == PARSER_NODE_BANG_EQUAL) type = IR_INSTRUCTION_TYPE_BANG_EQUAL;
    else if (node->type == PARSER_NODE_LESS_EQUAL) type = IR_INSTRUCTION_TYPE_LESS_EQUAL;
    else if (node->type == PARSER_NODE_GREATER_EQUAL) type = IR_INSTRUCTION_TYPE_GREATER_EQUAL;
    else if (node->type == PARSER_NODE_GREATER) type = IR_INSTRUCTION_TYPE_GREATER;
    else if (node->type == PARSER_NODE_LESS) type = IR_INSTRUCTION_TYPE_LESS;
    else type = IR_INSTRUCTION_TYPE_MUL;

    struct IR_Instruction *inst = IR_create_IR_Instruction(type);
    inst->destination = dest;
    inst->source_1 = left;
    inst->source_2 = right;
    IR_Block_add_instruction(builder->current_block, inst);
    return dest;
}

struct IR_Operand* IRLower_unary_op(struct IR_Builder *builder, struct parser_node *node){
    struct IR_Operand *right = IRLower_expression(builder, node->right_node);

    struct IR_Operand *dest = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, builder->current_function);
    dest->value.vreg_id = builder->current_function->vg_counter++;

    enum IR_InstructionType type;
    if (node->type == PARSER_NODE_UNARY_MINUS) type = IR_INSTRUCTION_TYPE_UNARY_MINUS;
    else if (node->type == PARSER_NODE_UNARY_BANG) type = IR_INSTRUCTION_TYPE_UNARY_BANG;
    else if (node->type == PARSER_NODE_UNARY_ADDRESS_OF) type = IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF;
    else if (node->type == PARSER_NODE_UNARY_DEREFERENCE) type = IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE;

    struct IR_Instruction *inst = IR_create_IR_Instruction(type);
    inst->destination = dest;
    inst->source_2 = right;
    IR_Block_add_instruction(builder->current_block, inst);

    return dest;
}


struct IR_Operand *IRL_new_vreg(struct IR_Builder *builder) {
    struct IR_Operand *op = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, builder->current_function);
    op->value.vreg_id = builder->current_function->vg_counter++; 
    return op;
}

