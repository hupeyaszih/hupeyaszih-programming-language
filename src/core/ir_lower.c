#include "core/ir_lower.h"
#include "core/ir_gen.h"
#include "core/parser.h"
#include "core/symbol_table.h"
#include "h_arena.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>



struct IR_Operand *IRL_create_stack_slot(struct arena *arena, struct IR_Function *function, struct type_info *type, struct IR_Instruction *definition_instruction, bool is_argument) {
    struct IR_Operand *stack_slot = IR_create_IR_Operand(arena, IR_OPERAND_TYPE_STACK_SLOT, definition_instruction, function, -1);
    struct stack_slot_t *slot = IR_create_stack_slot(arena, type, function, is_argument);

    stack_slot->data.slot.stack_slot = slot;
    stack_slot->type_info = type->pointer_type;
    slot->current_vreg = stack_slot;
    slot->is_busy = true;
    return stack_slot;
}

static inline void IRL_instruction_jmp_add_args(struct arena *arena, struct IR_Instruction *jmp, struct vector_t *args) {
    jmp->operands.jmp.args = vector_create_vector(arena, args->element_count, sizeof(struct symbol_t *));
    for(int i = 0; i < args->element_count; ++i) {
        struct IR_Operand *param = *(struct IR_Operand **) vector_get(args, i);
        if(IR_OPERAND_TYPE_VREG != param->type) continue;
        vector_add(jmp->operands.jmp.args, &param);
    }
}

static inline struct vector_t *IRL_block_filter(struct arena *arena, struct vector_t *mutated_variables, struct vector_t *declarated_variables) {
    struct vector_t *filtered = vector_create_vector(arena, 2, sizeof(struct IR_Operand *));
    for (int i = 0; i < mutated_variables->element_count; i++) {
        struct symbol_t *mutated_sym = *(struct symbol_t **) vector_get(mutated_variables, i);

        int is_local = 0;
        for (int j = 0; j < declarated_variables->element_count; j++) {
            struct symbol_t *local_sym = *(struct symbol_t **) vector_get(declarated_variables, j);

            if (mutated_sym == local_sym) {
                is_local = 1;
                break;
            }
        }

        if (!is_local) {
            vector_add(filtered, &mutated_sym->current_vreg);
        }
    }

    return filtered;
}

static inline void variable_declaration(struct ir_context *context, struct symbol_t *sym, struct parser_node *node) {
    if(LOCATION_STACK == sym->location_kind) {
        struct IR_Instruction *alloca = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_ALLOCA);
        IR_Block_add_instruction(context->current_block, alloca);

        struct IR_Operand *stack_slot = IRL_create_stack_slot(context->arena, context->current_function, sym->type, alloca, false);
        sym->stack_slot = stack_slot;

        alloca->operands.alloca.type_info = sym->type;

        alloca->operands.alloca.destination = stack_slot;
    }else {
        struct IR_Operand *vreg = IR_create_new_vreg(context->arena, context->current_function, NULL, sym, context->current_block->in_loop);
        vreg->type_info = sym->type;
        sym->current_vreg = vreg;
    }
}

static inline struct IR_Operand *load_variable(struct ir_context *context, struct symbol_t *sym) {
    enum location_kind location_kind = sym->location_kind;
    switch (location_kind) {
        case LOCATION_VREG: {
            return sym->current_vreg;
        }case LOCATION_STACK: {
            struct IR_Instruction *load = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_LOAD);
            IR_Block_add_instruction(context->current_block, load);

            struct IR_Operand *vreg = IR_create_new_vreg(context->arena, context->current_function, load, sym, context->current_block->in_loop);
            vreg->type_info = sym->type;

            load->operands.double_operands.source_1 = sym->stack_slot;
            load->operands.double_operands.destination = vreg;


            return vreg;
        }
    }
    return NULL;
}


static inline void emit_cast(struct ir_context *context, struct type_info *target_type, struct IR_Operand **val) {
    if((*val)->type_info->type_id == target_type->type_id) return;

    if(IR_OPERAND_TYPE_IMM == (*val)->type) return;

    if (type_table_can_that_promote_to((*val)->type_info, target_type)) {
        struct IR_Instruction *cast = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_CAST);
        IR_Block_add_instruction(context->current_block, cast);

        struct IR_Operand *cast_vreg = IR_create_new_vreg(context->arena, context->current_function, cast, NULL, context->current_block->in_loop);
        cast_vreg->type_info = target_type;

        cast->operands.double_operands.source_1 = *val;
        cast->operands.double_operands.destination = cast_vreg;

        *val = cast_vreg;
    }
}

static inline void store_variable(struct ir_context *context, struct symbol_t *sym, struct IR_Operand *val) {
    if (val->type_info != sym->type) {
        emit_cast(context, sym->type, &val);
    }

    enum location_kind location_kind = sym->location_kind;
    switch (location_kind) {
        case LOCATION_VREG: {
            if(val->type == IR_OPERAND_TYPE_VREG) {
                struct IR_Instruction *assign = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_MOV);
                IR_Block_add_instruction(context->current_block, assign);

                struct IR_Operand *vreg = IR_create_new_vreg(context->arena, context->current_function, assign, sym, context->current_block->in_loop);
                vreg->type_info = sym->type;

                assign->operands.double_operands.source_1 = val;
                assign->operands.double_operands.destination = vreg;

                sym->current_vreg = vreg;
            }else {
                struct IR_Instruction *assign = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_MOV);
                IR_Block_add_instruction(context->current_block, assign);

                struct IR_Operand *vreg = IR_create_new_vreg(context->arena, context->current_function, assign, sym, context->current_block->in_loop);
                vreg->type_info = sym->type;

                assign->operands.double_operands.source_1 = val;
                assign->operands.double_operands.destination = vreg;

                sym->current_vreg = vreg;
            }
            break;
        }case LOCATION_STACK: {
            struct IR_Instruction *store = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_STORE);
            IR_Block_add_instruction(context->current_block, store);

            store->operands.double_operands.source_1 = val;
            store->operands.double_operands.destination = sym->stack_slot;
            break;
        }
    }
}

static inline struct type_info *get_best_type_info_to_assign(struct type_info *left, struct type_info *right) {
    if (!left || !right) return NULL;
    
    if (left == right) return left;

    if (1 == type_table_can_that_promote_to(left, right)) {
        return right;
    } 
    if (1 == type_table_can_that_promote_to(right, left)) {
        return left;
    }

    return NULL;
}

struct IR_Operand *IRL_run_module_lower(struct parser_node *node, struct ir_context *context) {
    struct IR_Module *module = IR_create_IR_Module(context->arena, node->data.module.name);
    vector_add(context->project->modules, &module);
    module->parent_project = context->project;

    int function_count = node->data.module.functions->element_count;

    struct IR_Operand *last_operand = NULL;
    struct symbol_table *old_scope = context->current_scope;
    for(int i = 0;i < function_count; ++i) {
        struct parser_node *function_node = *(struct parser_node **) vector_get(node->data.module.functions, i);
        struct IR_Function *function = IR_create_IR_Function(context->arena, function_node->data.function.name, function_node->data.function.mangled_name, function_node->data.function.param_count);
        IR_Module_add_function(module, function);

        context->current_scope = old_scope;
        context->current_block = NULL;
        context->current_function = function;
        last_operand = IRL_run_function_lower(function_node, context);
    }
    arena_reset(context->temp_arena);
    return last_operand;
}

struct IR_Operand *IRL_run_function_lower(struct parser_node *node, struct ir_context *context) {
    struct symbol_t *sym = symbol_table_look_up(context->current_scope, node->data.function.name);
    sym->function.ir_function = context->current_function;

    struct IR_Block *params_b = IR_create_IR_Block(context->arena, context->current_function, node->data.function.params->data.block.mangled_name);
    context->current_block = params_b;
    IR_Function_add_block(context->current_function, params_b);



    struct symbol_table *last_scope = context->current_scope;
    context->current_scope = node->data.function.params->data.block.scope;

    struct parser_node *params = node->data.function.params;
    int param_count = params->data.block.count;
    for(int i = 0;i < param_count; ++i) {
        struct parser_node *param = *(struct parser_node **) vector_get(params->data.block.statements, i);
        struct IR_Operand *param_op = NULL;
        if(LOCATION_STACK == param->data.variable.symbol->location_kind) {
            // load arg to param
            struct symbol_t *var_sym = param->data.variable.symbol;
            struct IR_Operand *vreg = IR_create_new_vreg(context->arena, context->current_function, NULL, var_sym, params_b->in_loop);
            vreg->type_info = var_sym->type;
            param_op = vreg;
            IRL_run_statement_lower(param, context, LOWER_UNDEFINED);
            store_variable(context, var_sym, vreg);
        }else {
            param_op = IRL_run_statement_lower(param, context, LOWER_UNDEFINED);
        }

        vector_add(context->current_function->parameters, &param_op);
    }
    context->current_scope = last_scope;


    struct IR_Block *block = IR_create_IR_Block(context->arena, context->current_function, node->data.function.body->data.block.mangled_name);
    context->current_block = block;


    struct IR_Operand *return_value = IRL_run_block_lower(node->data.function.body, context);
    // emit_cast(context, node->data.function.return_type, &return_value);
    context->current_function->return_value = return_value;

    struct IR_Instruction *ret = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_RET);
    IR_Block_add_instruction(context->current_block, ret);
    ret->operands.ret.function = context->current_function;
    ret->operands.ret.return_value = return_value;


    // init instruction ids
    int instruction_id = 0;
    struct IR_Block *curr_block = context->current_function->head_block;
    while(NULL != curr_block) {
        struct IR_Instruction *curr_instruction = curr_block->head_instruction;
        while(NULL != curr_instruction) {
            curr_instruction->id = instruction_id;
            curr_instruction = curr_instruction->next;
            ++instruction_id;
        }
        curr_block = curr_block->next;
    }

    arena_reset(context->temp_arena);
    return return_value;
}

struct IR_Operand *IRL_run_block_lower(struct parser_node *node, struct ir_context *context) {
    struct symbol_table *last_scope = context->current_scope;

    struct IR_Block *block = context->current_block;
    IR_Function_add_block(context->current_function, block);
    context->current_scope = node->data.block.scope;

    block->parent_function = context->current_function;

    int statement_count = node->data.block.count;
    struct IR_Operand *last_operand = NULL;
    for(int i = 0;i < statement_count; ++i) {
        struct parser_node *curr = *(struct parser_node **) vector_get(node->data.block.statements, i);
        enum lower_type lower_type = LOWER_UNDEFINED;
        if(i == statement_count-1) lower_type = LOWER_R;
        last_operand = IRL_run_statement_lower(curr, context, lower_type);
    }




    context->current_scope = last_scope;
    return last_operand;
}

static inline struct IR_Operand *IRL_run_alu_lower(struct ir_context *context, struct parser_node *node) {

    struct IR_Operand *left_operand = NULL;
    struct IR_Operand *right_operand = NULL;
    if(node->left_node) left_operand = IRL_run_statement_lower(node->left_node, context, LOWER_R);
    if(node->right_node) right_operand = IRL_run_statement_lower(node->right_node, context, LOWER_R);

    enum IR_Instruction_type alu_type = IR_INSTRUCTION_TYPE_PLUS;
    switch (node->type) {
        case PARSER_NODE_PLUS:   {alu_type = IR_INSTRUCTION_TYPE_PLUS; break; }
        case PARSER_NODE_MINUS:  {alu_type = IR_INSTRUCTION_TYPE_MINUS; break; }
        case PARSER_NODE_DIVIDE: {alu_type = IR_INSTRUCTION_TYPE_DIVIDE; break; }
        case PARSER_NODE_MUL:    {alu_type = IR_INSTRUCTION_TYPE_MUL; break; }

        case PARSER_NODE_BANG_EQUAL:    {alu_type = IR_INSTRUCTION_TYPE_BANG_EQUAL; break; }
        case PARSER_NODE_EQUAL_EQUAL:   {alu_type = IR_INSTRUCTION_TYPE_EQUAL_EQUAL; break; }
        case PARSER_NODE_LESS_EQUAL:    {alu_type = IR_INSTRUCTION_TYPE_LESS_EQUAL; break; }
        case PARSER_NODE_LESS:          {alu_type = IR_INSTRUCTION_TYPE_LESS; break; }
        case PARSER_NODE_GREATER_EQUAL: {alu_type = IR_INSTRUCTION_TYPE_GREATER_EQUAL; break; }
        case PARSER_NODE_GREATER:       {alu_type = IR_INSTRUCTION_TYPE_GREATER; break; }
        default: return NULL;
    }

    struct type_info *dest_type_info = node->type_info;


    if(dest_type_info->type_id != left_operand->type_info->type_id) emit_cast(context, dest_type_info, &left_operand);
    if(dest_type_info->type_id != right_operand->type_info->type_id) emit_cast(context, dest_type_info, &right_operand);

    if(left_operand->type_info->type_id != right_operand->type_info->type_id) emit_cast(context, left_operand->type_info, &right_operand);
    if(left_operand->type_info->type_id != right_operand->type_info->type_id) emit_cast(context, right_operand->type_info, &left_operand);
    

    struct IR_Instruction *instruction = IR_create_IR_Instruction(context->arena, context->current_block, alu_type);
    IR_Block_add_instruction(context->current_block, instruction);

    struct IR_Operand *dest = IR_create_new_vreg(context->arena, context->current_function, instruction, NULL, context->current_block->in_loop);
    dest->type_info = dest_type_info;


    instruction->operands.triple_operands.destination = dest;
    instruction->operands.triple_operands.source_1 = left_operand;
    instruction->operands.triple_operands.source_2 = right_operand;

    return dest;
}


struct IR_Operand *IRL_run_statement_lower(struct parser_node *node, struct ir_context *context, enum lower_type lower_type) {
    if(NULL == node) return NULL;

    switch (node->type) {
        case PARSER_NODE_FUNCTION: {
            return IRL_run_function_lower(node, context);
        }case PARSER_NODE_BLOCK: {
            struct IR_Block *block = IR_create_IR_Block(context->arena, context->current_function, node->data.block.mangled_name);
            context->current_block = block;
            return IRL_run_block_lower(node, context);
        }case PARSER_NODE_LOOP: {
            struct vector_t *declared_variables = vector_create_vector(context->temp_arena, 2, sizeof(struct symbol_t *));
            struct vector_t *mutated_variables = vector_create_vector(context->temp_arena, 2, sizeof(struct symbol_t *));
            IRL_find_mutations(node->data.loop.body_block, mutated_variables, declared_variables);
            IRL_find_mutations(node->data.loop.continue_block, mutated_variables, declared_variables);

            for (int i = 0; i < mutated_variables->element_count; ++i) {
                struct symbol_t *mutated_sym = *(struct symbol_t **) vector_get(mutated_variables, i);
                if (mutated_sym->current_vreg && mutated_sym->current_vreg->type == IR_OPERAND_TYPE_VREG) {
                    mutated_sym->current_vreg->data.vreg.variable = mutated_sym;
                }
            }

            struct vector_t *entry_params = IRL_block_filter(context->temp_arena, mutated_variables, declared_variables);
            
            struct IR_Block *b_entry = context->current_block;

            struct IR_Block *b_body = IR_create_IR_Block(context->arena, context->current_function, node->data.loop.body_block->data.block.mangled_name);
            b_body->in_loop = b_entry->in_loop + 1;

            struct IR_Block *b_continue = IR_create_IR_Block(context->arena, context->current_function, node->data.loop.continue_block->data.block.mangled_name);
            b_continue->in_loop = b_entry->in_loop + 1;

            struct IR_Block *b_return = IR_create_IR_Block(context->arena, context->current_function, node->data.loop.return_block->data.block.mangled_name);
            b_return->in_loop = b_entry->in_loop;

            struct IR_Instruction *entry_jmp = IR_create_IR_Instruction(context->arena, b_entry, IR_INSTRUCTION_TYPE_JMP);
            entry_jmp->operands.jmp.target_block = b_body;
            IRL_instruction_jmp_add_args(context->arena, entry_jmp, entry_params);
            IR_Block_add_instruction(b_entry, entry_jmp);

            //
            for (int i = 0; i < entry_params->element_count; ++i) {
                struct IR_Operand *old_op = *(struct IR_Operand **) vector_get(entry_params, i);
                struct symbol_t *var = old_op->data.vreg.variable;
                if(IR_OPERAND_TYPE_STACK_SLOT == old_op->type) continue;
                

                struct IR_Operand *arg_op = IR_create_new_vreg(context->arena, b_body->parent_function, old_op->definition_instruction, var, context->current_block->in_loop);
                arg_op->type_info = var->type;
                var->current_vreg = arg_op;

                vector_add(b_body->params, &arg_op);
                
            }
            //


            // run lowering
            context->current_block = b_body;
            struct IR_Operand *body_res = IRL_run_block_lower(node->data.loop.body_block, context);
            struct IR_Block *body_end_b = context->current_block;

            struct vector_t *body_end_vregs = vector_create_vector(context->temp_arena, 2, sizeof(struct IR_Operand *));
            for (int i = 0; i < b_body->params->element_count; ++i) {
                struct IR_Operand *arg_op = *(struct IR_Operand **) vector_get(b_body->params, i);
                struct symbol_t *var = arg_op->data.vreg.variable;
                vector_add(body_end_vregs, &var->current_vreg);
            }


            context->current_block = b_continue;
            struct IR_Operand *continue_res = IRL_run_block_lower(node->data.loop.continue_block, context);
            struct IR_Block *continue_end_b = context->current_block;

            //
            struct vector_t *continue_params = IRL_block_filter(context->temp_arena,mutated_variables, declared_variables);
            struct IR_Instruction *jmp = IR_create_IR_Instruction(context->arena, b_continue, IR_INSTRUCTION_TYPE_JMP);
            jmp->operands.jmp.target_block = b_body;
            IRL_instruction_jmp_add_args(context->arena, jmp, continue_params);
            IR_Block_add_instruction(continue_end_b, jmp);
            //

            //
            for (int i = 0; i < b_body->params->element_count; ++i) {
                struct IR_Operand *arg_op = *(struct IR_Operand **) vector_get(b_body->params, i);
                struct symbol_t *var = arg_op->data.vreg.variable;
                struct IR_Operand *body_vreg = *(struct IR_Operand **) vector_get(body_end_vregs, i);
                var->current_vreg = body_vreg;

            }
            //


            context->current_block = b_return;
            struct IR_Operand *return_res = IRL_run_block_lower(node->data.loop.return_block, context);

            struct IR_Instruction *branch = IR_create_IR_Instruction(context->arena, body_end_b, IR_INSTRUCTION_TYPE_BR);
            IR_Block_add_instruction(body_end_b, branch);
            branch->operands.br.condition   = body_res;
            branch->operands.br.true_block  = b_continue;
            branch->operands.br.false_block = b_return;


            struct IR_Instruction *loop_head_instruction = b_body->head_instruction;
            struct IR_Instruction *loop_tail_instruction = b_continue->tail_instruction;
            
            b_body->loop_head_instruction = loop_head_instruction;
            b_body->loop_tail_instruction = loop_tail_instruction;

            b_continue->loop_head_instruction = loop_head_instruction;
            b_continue->loop_tail_instruction = loop_tail_instruction;

            b_return->loop_head_instruction = b_entry->loop_head_instruction;
            b_return->loop_tail_instruction = b_entry->loop_tail_instruction;

            return return_res;
        }case PARSER_NODE_NUMBER: {
            struct type_info *info = node->type_info;
            struct IR_Operand *operand = IR_create_IR_Operand(context->arena, IR_OPERAND_TYPE_IMM, NULL, context->current_function, context->current_block->in_loop);
            operand->type_info = info;
            operand->data.imm_value = node->data.literal_data;
            return operand;
        }case PARSER_NODE_STRING: {
            struct type_info *info = node->type_info;
            struct IR_Operand *operand = IR_create_IR_Operand(context->arena, IR_OPERAND_TYPE_IMM, NULL, context->current_function, context->current_block->in_loop);
            operand->type_info = info;
            operand->data.imm_value = node->data.literal_data;
            return operand;
        }case PARSER_NODE_CALL: {
            struct IR_Instruction *call = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_CALL);
            call->operands.call.arguments = vector_create_vector(context->arena, 2, sizeof(struct IR_Operand *));

            int arg_count = node->data.call.arg_count;
            struct str_view calling_function_name = node->data.call.name;
            struct symbol_t *calling_function = symbol_table_look_up(context->current_scope, calling_function_name);

            for(int i = 0;i < arg_count; ++i) {
                struct parser_node *arg = *(struct parser_node **) vector_get(node->data.call.args, i);
                struct IR_Operand *op = IRL_run_statement_lower(arg, context, LOWER_R);
                vector_add(call->operands.call.arguments, &op);

                emit_cast(context, arg->type_info, &op);
            }

            struct IR_Operand *return_val = IR_create_new_vreg(context->arena, context->current_function, call, NULL, context->current_block->in_loop);
            return_val->type_info = calling_function->function.return_type;

            call->operands.call.return_val = return_val;
            call->operands.call.target_function = calling_function->function.ir_function;

            IR_Block_add_instruction(context->current_block, call);
            return return_val;
        }case PARSER_NODE_UNARY_ADDRESS_OF: {
            struct IR_Operand *left_operand = NULL;
            struct IR_Operand *right_operand = NULL;
            if(node->left_node) left_operand = IRL_run_statement_lower(node->left_node, context, LOWER_L);
            if(node->right_node) right_operand = IRL_run_statement_lower(node->right_node, context, LOWER_L);

            struct IR_Instruction *instruction = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF);
            IR_Block_add_instruction(context->current_block, instruction);

            struct symbol_t *sym = node->right_node->data.variable.symbol;

            struct IR_Operand *destination = IR_create_new_vreg(context->arena, context->current_function, instruction, sym, context->current_block->in_loop);
            destination->type_info = type_table_get_type_info(context->type_table, destination->data.vreg.variable->type->name, destination->data.vreg.variable->pointer_level+1);

            instruction->operands.double_operands.source_1 = right_operand;
            instruction->operands.double_operands.destination = destination;

            return destination;
        }case PARSER_NODE_UNARY_DEREFERENCE: {
            struct IR_Operand *ptr_op = IRL_run_statement_lower(node->right_node, context, LOWER_R);

            if (lower_type == LOWER_R) {
                struct IR_Instruction *load_inst = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE);
                struct IR_Operand *dest = IR_create_new_vreg(context->arena, context->current_function, load_inst, node->right_node->data.variable.symbol, context->current_block->in_loop);
                dest->type_info = node->type_info;

                load_inst->operands.double_operands.destination = dest;
                load_inst->operands.double_operands.source_1 = ptr_op;
                IR_Block_add_instruction(context->current_block, load_inst);

                return dest;
            } else if (lower_type == LOWER_L) {
                return ptr_op;
            }
            break;
        }case PARSER_NODE_UNARY_BANG: {
            struct IR_Instruction *instruction = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_UNARY_BANG);

            struct IR_Operand *src = IRL_run_statement_lower(node->right_node, context, LOWER_R);

            struct IR_Operand *destination = IR_create_new_vreg(context->arena, context->current_function, instruction, src->data.vreg.variable, context->current_block->in_loop);
            destination->type_info = node->type_info;

            instruction->operands.double_operands.source_1 = src;
            instruction->operands.double_operands.destination = destination;

            IR_Block_add_instruction(context->current_block, instruction);
            return destination;
        }case PARSER_NODE_UNARY_MINUS: {
            struct IR_Instruction *instruction = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_UNARY_MINUS);

            struct IR_Operand *src = IRL_run_statement_lower(node->right_node, context, LOWER_R);

            struct IR_Operand *destination = IR_create_new_vreg(context->arena, context->current_function, instruction, src->data.vreg.variable, context->current_block->in_loop);
            destination->type_info = node->type_info;

            instruction->operands.double_operands.source_1 = src;
            instruction->operands.double_operands.destination = destination;

            IR_Block_add_instruction(context->current_block, instruction);
            return destination;
        }


        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_LESS:
        case PARSER_NODE_GREATER_EQUAL:
        case PARSER_NODE_GREATER:
        case PARSER_NODE_PLUS:
        case PARSER_NODE_MINUS:
        case PARSER_NODE_DIVIDE:
        case PARSER_NODE_MUL: 
        return IRL_run_alu_lower(context, node);

        case PARSER_NODE_IDENTIFIER: {
            struct symbol_t *sym = node->data.variable.symbol;

            if (lower_type == LOWER_L) {
                return (sym->location_kind == LOCATION_STACK) ? sym->stack_slot : sym->current_vreg;
            } else {

                return load_variable(context, sym);
            }
        }case PARSER_NODE_VARIABLE_DECLARATION: {
            struct IR_Operand *left_operand = NULL;
            struct IR_Operand *right_operand = NULL;
            if(node->left_node) left_operand = IRL_run_statement_lower(node->left_node, context, LOWER_L);
            if(node->right_node) right_operand = IRL_run_statement_lower(node->right_node, context, LOWER_R);

            struct symbol_t *sym = node->data.variable.symbol;
            variable_declaration(context, sym, node);


            if(right_operand) {
                store_variable(context, sym, right_operand);
            }
            return node->data.variable.symbol->current_vreg;
        }case PARSER_NODE_VARIABLE_ASSIGMENT: {
            struct IR_Operand *right_operand = IRL_run_statement_lower(node->right_node, context, LOWER_R);

            if (node->left_node->type == PARSER_NODE_UNARY_DEREFERENCE) {
                struct IR_Operand *ptr_operand = IRL_run_statement_lower(node->left_node, context, LOWER_L);

                struct IR_Instruction *store= IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_STORE);

                store->operands.double_operands.destination = ptr_operand;
                store->operands.double_operands.source_1 = right_operand; 

                IR_Block_add_instruction(context->current_block, store);

                return right_operand;
            } else if (node->left_node->type == PARSER_NODE_IDENTIFIER) {
                struct symbol_t *sym = node->left_node->data.variable.symbol;
                store_variable(context, sym, right_operand);
                return right_operand;
            }

            break;
        }case PARSER_NODE_ASM: {
            struct IR_Instruction *instruction = IR_create_IR_Instruction(context->arena, context->current_block, IR_INSTRUCTION_TYPE_ASM);
            IR_Block_add_instruction(context->current_block, instruction);

            instruction->operands.asm_operands.asm_imm = node->data.literal_data;
            return NULL;
        }
    }
    return NULL;
}

int IRL_build_ir(struct IR_Project *project, struct parser_t *parser) {
    struct ir_context context;
    context.project = project;
    context.type_table = parser->type_table;
    context.error = 0;
    context.arena = project->arena;
    context.temp_arena = project->temp_arena;


    int module_count = parser->nodes->element_count;
    for(int i = 0; i < module_count; ++i) {
        context.current_block = NULL;
        context.current_function = NULL;
        context.current_scope = parser->current_scope;
        struct parser_node *node = *(struct parser_node **) vector_get(parser->nodes, i);
        IRL_run_module_lower(node, &context);
    }


    return context.error;
}

static inline void add_mutated_var(struct vector_t *mutated_vars, const struct symbol_t *new_var) {
    if (!new_var) return;

    for (int i = 0; i < mutated_vars->element_count; ++i) {
        struct symbol_t *var = *(struct symbol_t **) vector_get(mutated_vars, i);
        if (var == new_var) return;
    }
    vector_add(mutated_vars, &new_var);
}

void IRL_find_mutations(struct parser_node *node, struct vector_t *vars, struct vector_t *declarated_vars) {
    if (!node) return;

    switch (node->type) {
        case PARSER_NODE_VARIABLE_DECLARATION: {
            if (node->data.variable.symbol) {
                vector_add(declarated_vars, &node->data.variable.symbol);
            }
            IRL_find_mutations(node->right_node, vars, declarated_vars);
            break;
        } case PARSER_NODE_VARIABLE_ASSIGMENT: {
            if (node->left_node) {
                IRL_find_mutations(node->left_node, vars, declarated_vars);
            }

            if (node->left_node && node->left_node->type == PARSER_NODE_IDENTIFIER) {
                struct symbol_t *sym = node->data.variable.symbol;
                add_mutated_var(vars, sym);
            }
            IRL_find_mutations(node->right_node, vars, declarated_vars);
            break;
        }case PARSER_NODE_BLOCK: {
            if (node->data.block.statements) {
                for (int i = 0; i < node->data.block.count; i++) {
                    struct parser_node *statement = *(struct parser_node **) vector_get(node->data.block.statements, i);
                    IRL_find_mutations(statement, vars, declarated_vars);
                }
            }
            break;
        }
        case PARSER_NODE_PLUS:
        case PARSER_NODE_MINUS:
        case PARSER_NODE_MUL:
        case PARSER_NODE_DIVIDE:
        case PARSER_NODE_LESS:
        case PARSER_NODE_GREATER:
        case PARSER_NODE_EQUAL_EQUAL:
        case PARSER_NODE_BANG_EQUAL:
        case PARSER_NODE_LESS_EQUAL:
        case PARSER_NODE_GREATER_EQUAL: {
            IRL_find_mutations(node->left_node, vars, declarated_vars);
            IRL_find_mutations(node->right_node, vars, declarated_vars);
            break;
        }

        case PARSER_NODE_UNARY_BANG:
        case PARSER_NODE_UNARY_MINUS:
        case PARSER_NODE_UNARY_ADDRESS_OF:
        case PARSER_NODE_UNARY_DEREFERENCE: {
            IRL_find_mutations(node->left_node, vars, declarated_vars);
            break;
        }

        case PARSER_NODE_CALL: {
            if (node->data.call.args) {
                for (int i = 0; i < node->data.call.arg_count; i++) {
                    struct parser_node *arg_node = *(struct parser_node **) vector_get(node->data.call.args, i);
                    IRL_find_mutations(arg_node, vars, declarated_vars);
                }
            }
            break;
        }

        case PARSER_NODE_LOOP: {
            IRL_find_mutations(node->data.loop.body_block, vars, declarated_vars);
            IRL_find_mutations(node->data.loop.return_block, vars, declarated_vars);
            IRL_find_mutations(node->data.loop.continue_block, vars, declarated_vars);
            break;
        }

        default:
            break;
    }
}
