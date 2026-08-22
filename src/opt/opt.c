#include "opt/opt.h"
#include "backend/codegen.h"
#include "core/flags/function_flags.h"
#include "core/ir_gen.h"
#include "h_arena.h"
#include "h_bitset.h"
#include "h_string_view.h"
#include "h_vector.h"
#include "opt/register_allocator.h"
#include <stdint.h>
#include <stdio.h>

static inline struct vector_t *get_used_operands(struct arena *arena, struct IR_Instruction *instruction) {
    struct vector_t *list = vector_create_vector(arena, 3, sizeof(struct IR_Operand *));

    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_LOAD:{
            vector_add(list, &instruction->operands.double_operands.source_1);
            break;
        }case IR_INSTRUCTION_TYPE_STORE: {
            vector_add(list, &instruction->operands.double_operands.source_1);
            vector_add(list, &instruction->operands.double_operands.destination);
            break;
        }
        case IR_INSTRUCTION_TYPE_BITWISE_AND:
        case IR_INSTRUCTION_TYPE_BITWISE_OR:
        case IR_INSTRUCTION_TYPE_BITWISE_XOR:
        case IR_INSTRUCTION_TYPE_SHR:
        case IR_INSTRUCTION_TYPE_SHL:
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_MOD:
        case IR_INSTRUCTION_TYPE_MUL: {
            vector_add(list, &instruction->operands.triple_operands.source_1);
            vector_add(list, &instruction->operands.triple_operands.source_2);
            break;
        }case IR_INSTRUCTION_TYPE_BR: {
            vector_add(list, &instruction->operands.br.condition);
            struct vector_t *false_args = instruction->operands.br.false_args;
            if(false_args) {
                for(int i = 0; i < false_args->element_count; ++i) {
                    struct IR_Operand *arg = *(struct IR_Operand **) vector_get(false_args, i);
                    vector_add(list, &arg);
                }
            }

            struct vector_t *true_args = instruction->operands.br.true_args;
            if(true_args) {
                for(int i = 0; i < true_args->element_count; ++i) {
                    struct IR_Operand *arg = *(struct IR_Operand **) vector_get(true_args, i);
                    vector_add(list, &arg);
                }
            }
            break;
        }case IR_INSTRUCTION_TYPE_JMP: {
            if(!instruction->operands.jmp.args) break;
            struct vector_t *args = instruction->operands.jmp.args;
            for(int i = 0; i < args->element_count; ++i) {
                struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
                vector_add(list, &arg);
            }
            break;
        }case IR_INSTRUCTION_TYPE_RET: {
            vector_add(list, &instruction->operands.ret.return_value);
            break;
        }case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_UNARY_NOT:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            vector_add(list, &instruction->operands.double_operands.source_1);
            break;
        }case IR_INSTRUCTION_TYPE_CALL: {
           struct vector_t *args = instruction->operands.call.arguments;
           int arg_count = args->element_count;

           for(int i = 0;i < arg_count; ++i) {
               struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
               vector_add(list, &arg);
           }
           break;
        }
        case IR_INSTRUCTION_TYPE_ALLOCA: break;
        case IR_INSTRUCTION_TYPE_ASM: break;
        case IR_INSTRUCTION_TYPE_NOP: break;
        case IR_INSTRUCTION_TYPE_UNDEFINED: break;
    }

    return list;
}
static inline struct vector_t *get_defined_operands(struct arena *arena, struct IR_Instruction *instruction) {
    struct vector_t *list = vector_create_vector(arena, 3, sizeof(struct IR_Operand *));

    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_LOAD: {
            vector_add(list, &instruction->operands.double_operands.destination);
            break;
        }case IR_INSTRUCTION_TYPE_ALLOCA: {
            vector_add(list, &instruction->operands.alloca.destination);
            break;
        }case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_BITWISE_AND:
        case IR_INSTRUCTION_TYPE_BITWISE_OR:
        case IR_INSTRUCTION_TYPE_BITWISE_XOR:
        case IR_INSTRUCTION_TYPE_SHR:
        case IR_INSTRUCTION_TYPE_SHL:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_MOD:
        case IR_INSTRUCTION_TYPE_MUL: {
            vector_add(list, &instruction->operands.triple_operands.destination);
            break;
        }case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_UNARY_NOT:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            vector_add(list, &instruction->operands.double_operands.destination);
            break;
        }case IR_INSTRUCTION_TYPE_CALL: {
           vector_add(list, &instruction->operands.call.return_val);
           break;
        }case IR_INSTRUCTION_TYPE_JMP: {
            if(!instruction->operands.jmp.target_block->params) break;
            struct vector_t *params = instruction->operands.jmp.target_block->params;
            for(int i = 0; i < params->element_count; ++i) {
                struct IR_Operand *arg = *(struct IR_Operand **) vector_get(params, i);
                vector_add(list, &arg);
            }
            break;
        }case IR_INSTRUCTION_TYPE_BR: {
            struct vector_t *false_params = instruction->operands.br.false_block->params;
            if(false_params) {
                for(int i = 0; i < false_params->element_count; ++i) {
                    struct IR_Operand *arg = *(struct IR_Operand **) vector_get(false_params, i);
                    vector_add(list, &arg);
                }
            }

            struct vector_t *true_params = instruction->operands.br.true_block->params;
            if(true_params) {
                for(int i = 0; i < true_params->element_count; ++i) {
                    struct IR_Operand *arg = *(struct IR_Operand **) vector_get(true_params, i);
                    vector_add(list, &arg);
                }
            }
            break;
        }default: {
            break;
        }
    }

    return list;
}

void opt_optimize_project(struct IR_Project *restrict project, struct codegen_t *codegen, enum optimization_level opt_level) {
    if(!project || !codegen) return;

    struct opt_context_t opt_context;
    opt_context.opt_level = opt_level;
    opt_context.register_allocator = register_allocator_create_register_allocator(codegen);
    opt_context.codegen = codegen;
    opt_context.temp_arena = codegen->temp_arena;

    for(int i = 0;i < project->modules->element_count; ++i) {
        struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
        opt_optimize_module(&opt_context, module);
    }
}

void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module) {
    if(!module) return;

    for(int i = 0;i < module->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **) vector_get(module->functions, i);
        //
        opt_run_cfg_analysis(function);
        opt_compute_use_def(context->codegen->arena, function);

        // optimizations

        opt_calculate_constants(function, context);
        bool changed = true;
        if(context->opt_level >= OPT_LEVEL_O1) {
            while (changed) {
                changed = false;
                changed |= opt_constant_folding     (context, function);
                changed |= opt_copy_propagation     (context, function);
                changed |= opt_dead_code_elimination(context, function);
            }
        }

        //

        opt_run_live_range_analysis(function, context);
        register_allocator_run_allocator(context->register_allocator, function);
        //
    }

    register_allocator_compute_caller_saved_registers(context->codegen->arena, context->codegen->current_build_target, module->parent_project->main_function);

    arena_reset(context->temp_arena);
}

void opt_run_cfg_analysis(struct IR_Function *function) {
    struct IR_Block *block = function->head_block;
    struct IR_Instruction *tail_instruction = NULL;

    while(NULL != block) {
        tail_instruction = block->tail_instruction;
        if(NULL == tail_instruction) {
            if (NULL != block->next) {
                vector_add(block->successors, &block->next);
                vector_add(block->next->predecessor, &block);
            }
            block = block->next;
            continue;
        }

        switch (tail_instruction->type) {
            case IR_INSTRUCTION_TYPE_JMP: {
                struct IR_Block *target_block = tail_instruction->operands.jmp.target_block;
                vector_add(block->successors, &target_block);
                vector_add(target_block->predecessor, &block);
                break;
            }case IR_INSTRUCTION_TYPE_BR: {
                struct IR_Block *true_block  = tail_instruction->operands.br.true_block;
                struct IR_Block *false_block = tail_instruction->operands.br.false_block;

                vector_add(block->successors, &true_block);
                vector_add(block->successors, &false_block);
                vector_add(true_block->predecessor , &block);
                vector_add(false_block->predecessor, &block);
                break;
            }case IR_INSTRUCTION_TYPE_RET: {
                break;
            }default: {
                vector_add(block->successors, &block->next);
                if(block->next)vector_add(block->next->predecessor, &block);
                break;
            }
        }

        block = block->next;
    }
}

static inline void process_use(const struct IR_Operand *operand, struct bitset_t *use, struct IR_Instruction *instruction) {
    if(!operand || IR_OPERAND_TYPE_VREG != operand->type) return;

    bitset_set(use, operand->data.vreg.vreg_id);

    vector_add(operand->use_list, &instruction);
}

static inline void process_def(const struct IR_Operand *operand, struct bitset_t *def) {
    if(!operand || IR_OPERAND_TYPE_VREG != operand->type) return;

    bitset_set(def, operand->data.vreg.vreg_id);
}

void opt_compute_use_def(struct arena *arena, struct IR_Function *function) {
    int vreg_count = function->unique_vregs->element_count;

    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        block->uses       = bitset_create(arena, vreg_count);
        block->defs       = bitset_create(arena, vreg_count);

        while(NULL != instruction) {
            switch (instruction->type) {
                case IR_INSTRUCTION_TYPE_MOV:
                case IR_INSTRUCTION_TYPE_LOAD: {
                    process_use(instruction->operands.double_operands.source_1, block->uses, instruction);
                    process_def(instruction->operands.double_operands.destination, block->defs);
                    break;
                }case IR_INSTRUCTION_TYPE_ALLOCA: {
                    process_def(instruction->operands.alloca.destination, block->defs);
                    break;
                }case IR_INSTRUCTION_TYPE_STORE: {
                    process_use(instruction->operands.double_operands.source_1, block->uses, instruction);
                    process_use(instruction->operands.double_operands.destination, block->uses, instruction);
                    break;
                }
                case IR_INSTRUCTION_TYPE_BITWISE_AND:
                case IR_INSTRUCTION_TYPE_BITWISE_OR:
                case IR_INSTRUCTION_TYPE_BITWISE_XOR:
                case IR_INSTRUCTION_TYPE_SHL:
                case IR_INSTRUCTION_TYPE_SHR:
                case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
                case IR_INSTRUCTION_TYPE_BANG_EQUAL:
                case IR_INSTRUCTION_TYPE_LESS_EQUAL:
                case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
                case IR_INSTRUCTION_TYPE_LESS:
                case IR_INSTRUCTION_TYPE_GREATER:
                case IR_INSTRUCTION_TYPE_PLUS:
                case IR_INSTRUCTION_TYPE_MINUS:
                case IR_INSTRUCTION_TYPE_DIVIDE:
                case IR_INSTRUCTION_TYPE_MOD:
                case IR_INSTRUCTION_TYPE_MUL: {
                    process_use(instruction->operands.triple_operands.source_1, block->uses, instruction);
                    process_use(instruction->operands.triple_operands.source_2, block->uses, instruction);
                    process_def(instruction->operands.triple_operands.destination, block->defs);
                    break;
                }
                case IR_INSTRUCTION_TYPE_BR: {
                    process_use(instruction->operands.br.condition, block->uses, instruction);

                    struct vector_t *false_args = instruction->operands.br.false_args;
                    if(false_args) {
                        for(int i = 0; i < false_args->element_count; ++i) {
                            struct IR_Operand *arg = *(struct IR_Operand **) vector_get(false_args, i);
                            process_use(arg, block->uses, instruction);
                        }
                    }

                    struct vector_t *true_args = instruction->operands.br.true_args;
                    if(true_args) {
                        for(int i = 0; i < true_args->element_count; ++i) {
                            struct IR_Operand *arg = *(struct IR_Operand **) vector_get(true_args, i);
                            process_use(arg, block->uses, instruction);
                        }
                    }

                    break;
                }case IR_INSTRUCTION_TYPE_JMP: {
                    if(!instruction->operands.jmp.args) break;
                    struct vector_t *args = instruction->operands.jmp.args;
                    for(int i = 0; i < args->element_count; ++i) {
                        struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
                        process_use(arg, block->uses, instruction);
                    }
                    break;
                }case IR_INSTRUCTION_TYPE_RET: {
                    process_use(instruction->operands.ret.return_value, block->uses, instruction);
                    break;
                }
                case IR_INSTRUCTION_TYPE_CAST:
                case IR_INSTRUCTION_TYPE_UNARY_NOT:
                case IR_INSTRUCTION_TYPE_UNARY_BANG:
                case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
                case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
                case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
                    process_use(instruction->operands.double_operands.source_1, block->uses, instruction);
                    process_def(instruction->operands.double_operands.destination, block->defs);
                    break;
                }
                case IR_INSTRUCTION_TYPE_CALL: {
                    struct vector_t *args = instruction->operands.call.arguments;
                    int arg_count = args->element_count;

                    for(int i = 0;i < arg_count; ++i) {
                        struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
                        process_use(arg, block->uses, instruction);
                    }

                    process_def(instruction->operands.call.return_val, block->defs);
                    break;
                }
                case IR_INSTRUCTION_TYPE_ASM:
                case IR_INSTRUCTION_TYPE_UNDEFINED:
                case IR_INSTRUCTION_TYPE_NOP: break;
            }

            instruction = instruction->next;
        }
        block = block->next;
    }
}
void opt_compute_local_liveness(struct IR_Block *block, int vreg_count, struct arena *arena ,struct arena *temp_arena) {
    block->uses     = bitset_create(arena, vreg_count);
    block->defs     = bitset_create(arena, vreg_count);
    block->live_in  = bitset_create(arena, vreg_count);
    block->live_out = bitset_create(arena, vreg_count);

    struct IR_Instruction *inst = block->head_instruction;
    while (inst) {
        struct vector_t *used_ops = get_used_operands(temp_arena, inst);
        struct vector_t *def_ops  = get_defined_operands(temp_arena, inst);

        for (int i = 0; i < used_ops->element_count; ++i) {
            struct IR_Operand *op = *(struct IR_Operand **) vector_get(used_ops, i);
            if (op && IR_OPERAND_TYPE_VREG == op->type) {
                int id = op->data.vreg.vreg_id;
                if (!bitset_test(block->defs, id)) {
                    bitset_set(block->uses, id);
                }
            }
        }

        for (int i = 0; i < def_ops->element_count; ++i) {
            struct IR_Operand *op = *(struct IR_Operand **) vector_get(def_ops, i);
            if (op && IR_OPERAND_TYPE_VREG == op->type) {
                bitset_set(block->defs, op->data.vreg.vreg_id);
            }
        }

        arena_reset(temp_arena);
        inst = inst->next;
    }
}

void opt_run_global_liveness(struct IR_Function *function, struct opt_context_t *context) {
    int vreg_count = function->unique_vregs->element_count;
    struct arena *arena = context->codegen->arena;
    struct arena *temp_arena = context->temp_arena;
    struct IR_Block *block = function->tail_block;
    while (block) {
        opt_compute_local_liveness(block, vreg_count, arena, temp_arena);
        block = block->prev;
    }

    bool changed = true;
    while (changed) {
        changed = false;

        block = function->tail_block;
        while (block) {
            struct bitset_t *old_live_in = bitset_create(temp_arena, vreg_count);
            bitset_copy(old_live_in, block->live_in);

            bitset_clear_all(block->live_out);
            for (int i = 0; i < block->successors->element_count; ++i) {
                struct IR_Block *succ = *(struct IR_Block **) vector_get(block->successors, i);
                bitset_or(block->live_out, succ->live_in);
            }

            struct bitset_t *temp = bitset_copy_to_temp(temp_arena, block->live_out);

            bitset_and_not(temp, block->defs);

            bitset_copy(block->live_in, block->uses);
            bitset_or(block->live_in, temp);

            if (!bitset_equals(block->live_in, old_live_in)) {
                changed = true;
            }

            arena_reset(temp_arena);
            block = block->prev;
        }
    }
    arena_reset(temp_arena);
}

void opt_run_live_range_analysis(struct IR_Function *function, struct opt_context_t *context) {
    int vreg_count = function->unique_vregs->element_count;
    struct arena *temp_arena = context->temp_arena;

    for(int i = 0; i < function->parameters->element_count; ++i) {
        struct IR_Operand *param = *(struct IR_Operand **) vector_get(function->parameters, i);
        if (param && IR_OPERAND_TYPE_VREG == param->type) {
            param->data.vreg.live_interval.start = 0;
        }
    }

    opt_run_global_liveness(function, context);

    struct IR_Block *block = function->tail_block;
    while(NULL != block) {
        if (NULL != block->head_instruction) {
            struct IR_Instruction *instruction = block->tail_instruction;
            while(NULL != instruction) {
                if(IR_INSTRUCTION_TYPE_NOP == instruction->type) {
                    instruction = instruction->prev;
                    continue;
                }

                struct vector_t *used_operands    = get_used_operands(temp_arena, instruction);
                struct vector_t *defined_operands = get_defined_operands(temp_arena, instruction);

                for(int i = 0; i < used_operands->element_count; ++i) {
                    struct IR_Operand *operand = *(struct IR_Operand **) vector_get(used_operands, i);
                    if(!operand || IR_OPERAND_TYPE_VREG != operand->type) continue;
                    operand->data.vreg.live_interval.use_score += block->in_loop * 10 + 1;

                    if(0 < block->in_loop && block->loop_tail_instruction) {
                        int loop_end_id = block->loop_tail_instruction->id;
                        if (operand->data.vreg.live_interval.end < loop_end_id) {
                            operand->data.vreg.live_interval.end = loop_end_id;
                        }
                    }
                    if (operand->data.vreg.live_interval.end < instruction->id) {
                        operand->data.vreg.live_interval.end = instruction->id;
                    }
                }

                for(int i = 0; i < defined_operands->element_count; ++i) {
                    struct IR_Operand *operand = *(struct IR_Operand **) vector_get(defined_operands, i);
                    if(!operand || IR_OPERAND_TYPE_VREG != operand->type) continue;
                    operand->data.vreg.live_interval.start = instruction->id;

                    if(operand->data.vreg.live_interval.end < instruction->id) {
                        operand->data.vreg.live_interval.end = instruction->id;
                    }
                }

                instruction = instruction->prev;
            }
        }

        int block_start_id = block->head_instruction ? block->head_instruction->id : 0;
        int block_end_id = block->tail_instruction ? block->tail_instruction->id : block_start_id;

        for (int id = 0; id < vreg_count; ++id) {
            if (bitset_test(block->live_in, id)) {
                struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(function->unique_vregs, id);
                if (vreg && vreg->data.vreg.live_interval.start > block_start_id) {
                    vreg->data.vreg.live_interval.start = block_start_id;
                }
            }
            if (bitset_test(block->live_out, id)) {
                struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(function->unique_vregs, id);
                if (vreg && vreg->data.vreg.live_interval.end < block_end_id) {
                    vreg->data.vreg.live_interval.end = block_end_id;
                }
            }
        }

        block = block->prev;
    }

    for(int i = 0;i < function->operands->element_count; ++i) {
        struct IR_Operand *operand = *(struct IR_Operand **) vector_get(function->operands, i);
        if(!operand || (IR_OPERAND_TYPE_STACK_SLOT != operand->type && IR_OPERAND_TYPE_VREG != operand->type)) continue;
        if(IR_OPERAND_TYPE_VREG == operand->type) {
            int start     = operand->data.vreg.live_interval.start;
            int end       = operand->data.vreg.live_interval.end;
            int use_score = operand->data.vreg.live_interval.use_score;
            int live_len  = end - start;
            operand->data.vreg.live_interval.weight = (use_score * 1000) / (live_len > 0 ? live_len : 1);
        }else if(IR_OPERAND_TYPE_STACK_SLOT == operand->type) {
            int start     = operand->data.slot.live_interval.start;
            int end       = operand->data.slot.live_interval.end;
            int use_score = operand->data.slot.live_interval.use_score;
            int live_len  = end - start;
            operand->data.slot.live_interval.weight = (use_score * 1000) / (live_len > 0 ? live_len : 1);
        }
    }

    arena_reset(temp_arena);
}

bool get_operand_imm_value(struct IR_Operand *op, int64_t *out_val) {
    if (!op) return false;

    if (op->type == IR_OPERAND_TYPE_IMM) {
        *out_val = str_view_to_int(op->data.imm_value);
        return true;
    }

    if (op->type == IR_OPERAND_TYPE_GLOBAL) {
        if(op->data.global.kind == IR_GLOBAL_KIND_STRING) return false;
        *out_val = str_view_to_int(op->data.global.value);
        return true;
    }

    if (op->type == IR_OPERAND_TYPE_VREG && op->definition_instruction) {
        struct IR_Instruction *def_inst = op->definition_instruction;

        if (def_inst->type == IR_INSTRUCTION_TYPE_MOV) {
            struct IR_Operand *src = def_inst->operands.double_operands.source_1;
            
            return get_operand_imm_value(src, out_val);
        }
    }

    return false;
}

static inline bool instruction_has_side_effects(struct IR_Instruction *inst) {
    if (!inst) return true;

    switch (inst->type) {
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE:
        case IR_INSTRUCTION_TYPE_UNARY_NOT:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS:
        case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_MOD:
        case IR_INSTRUCTION_TYPE_MUL:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_BITWISE_AND:
        case IR_INSTRUCTION_TYPE_BITWISE_OR:
        case IR_INSTRUCTION_TYPE_BITWISE_XOR:
        case IR_INSTRUCTION_TYPE_SHR:
        case IR_INSTRUCTION_TYPE_SHL:
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_LESS:
            return false;
        case IR_INSTRUCTION_TYPE_CALL: {
            if(1 == bitset_test(inst->operands.call.target_function->flags, FUNC_FLAG_IS_PURE)) {
                return false;
            }
            return true;
        }case IR_INSTRUCTION_TYPE_LOAD: {
            if(inst->operands.double_operands.source_1->type == IR_OPERAND_TYPE_GLOBAL) return true;
            if(inst->operands.double_operands.destination->type == IR_OPERAND_TYPE_GLOBAL) return true;
            return false;
        }

        default:
            return true;
    }
}

static inline void remove_instruction_from_use_list(struct IR_Operand *op, struct IR_Instruction *inst) {
    if (!op || op->type != IR_OPERAND_TYPE_VREG || !op->use_list) return;

    for (int i = 0; i < op->use_list->element_count; ++i) {
        struct IR_Instruction *use_inst = *(struct IR_Instruction **) vector_get(op->use_list, i);
        if (use_inst == inst) {
            vector_remove_at(op->use_list, i);
            break;
        }
    }
}

bool opt_dead_code_elimination(struct opt_context_t *context, struct IR_Function *function) {
    bool changed = false;

    struct IR_Block *block = function->head_block; 
    while (NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        while (NULL != instruction) {
            struct IR_Instruction *next_instruction = instruction->next;

            struct IR_Operand *dest = NULL;
            struct IR_Operand *src1 = NULL;
            struct IR_Operand *src2 = NULL;
            enum IR_Instructions_Operands_type ops_type = IR_get_Instructions_Operands_type(instruction->type);
            switch (ops_type) {
                case IR_INSTRUCTIONS_OPERANDS_TYPE_DOUBLE: dest = instruction->operands.double_operands.destination; src1 = instruction->operands.double_operands.source_1; src2 = NULL; break;
                case IR_INSTRUCTIONS_OPERANDS_TYPE_TRIPLE: dest = instruction->operands.triple_operands.destination; src1 = instruction->operands.triple_operands.source_1; src2 = instruction->operands.triple_operands.source_2; break;
                case IR_INSTRUCTIONS_OPERANDS_TYPE_ALLOCA: dest = instruction->operands.alloca.destination;          src1 = NULL; src2 = NULL; break;
                case IR_INSTRUCTIONS_OPERANDS_TYPE_CALL:   dest = instruction->operands.call.return_val;             src1 = NULL; src2 = NULL; break;
                case IR_INSTRUCTIONS_OPERANDS_TYPE_RET:    dest = NULL;                                              src1 = instruction->operands.ret.return_value; src2 = NULL; break;
                default: break;
            }

            bool instruction_removed = false;

            if (!instruction_has_side_effects(instruction)) {

                if (dest && dest->type == IR_OPERAND_TYPE_VREG && dest->use_list->element_count == 0) {
                    

                    if(src1)remove_instruction_from_use_list(src1, instruction);
                    if(src2)remove_instruction_from_use_list(src2, instruction);

                    IR_Block_remove_instruction(block, instruction);
                    if(src1 && 0 >= src1->use_list->element_count) {
                        src1->type = IR_OPERAND_TYPE_UNDEFINED;
                    }
                    if(src2 && 0 >= src2->use_list->element_count) {
                        src2->type = IR_OPERAND_TYPE_UNDEFINED;
                    }
                    if(dest && 0 >= dest->use_list->element_count) {
                        dest->type = IR_OPERAND_TYPE_UNDEFINED;
                    }
                    changed = true;
                    instruction_removed = true;
                }
            }
            if(!instruction_removed) {
                if((dest && dest->type == IR_OPERAND_TYPE_UNDEFINED) || (src1 && src1->type == IR_OPERAND_TYPE_UNDEFINED) || (src2 && src2->type == IR_OPERAND_TYPE_UNDEFINED)) {
                    if(src1)remove_instruction_from_use_list(src1, instruction);
                    if(src2)remove_instruction_from_use_list(src2, instruction);

                    if(src1 && 0 >= src1->use_list->element_count) {
                        src1->type = IR_OPERAND_TYPE_UNDEFINED;
                    }
                    if(src2 && 0 >= src2->use_list->element_count) {
                        src2->type = IR_OPERAND_TYPE_UNDEFINED;
                    }

                    if(dest) {
                        dest->type = IR_OPERAND_TYPE_UNDEFINED;
                    }
                    IR_Block_remove_instruction(block, instruction);
                    changed = true;
                }
            }
            instruction = next_instruction;
        }
        block = block->next;
    }
    return changed;
}


static inline bool is_block_argument(struct IR_Function *function, struct IR_Operand *op) {
    if (op->type != IR_OPERAND_TYPE_VREG) return false;

    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        for (int j = 0; j < block->params->element_count; ++j) {
            struct IR_Operand *arg_op = *(struct IR_Operand **) vector_get(block->params, j);

            if (arg_op == op) {
                return true;
            }
        }
        block = block->next;
    }
    return false;
}

void opt_calculate_constants(struct IR_Function *function, struct opt_context_t *context) {
    for(int i = 0; i < function->operands->element_count; ++i) {
        struct IR_Operand *op = *(struct IR_Operand **) vector_get(function->operands, i);
        if(op->constant) continue;
        if(IR_OPERAND_TYPE_IMM == op->type) {
            op->constant = true;
        }else if(IR_OPERAND_TYPE_VREG == op->type && op->definition_instruction) {
            if (is_block_argument(function, op))continue;
            struct IR_Instruction *def_inst = op->definition_instruction;
            if (def_inst->type == IR_INSTRUCTION_TYPE_MOV) {
                struct IR_Operand *src = def_inst->operands.double_operands.source_1;
                if(src->type == IR_OPERAND_TYPE_IMM) op->constant = true;
            }
        }
    }
}



bool opt_constant_folding(struct opt_context_t *context, struct IR_Function *function){
    bool changed = false;

    struct IR_Block *block = function->head_block; 
    while (NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        while (NULL != instruction) {
            enum IR_Instructions_Operands_type ops_type = IR_get_Instructions_Operands_type(instruction->type);
            if(ops_type == IR_INSTRUCTIONS_OPERANDS_TYPE_TRIPLE) {
                //
                struct IR_Operand *src1 = instruction->operands.triple_operands.source_1;
                struct IR_Operand *src2 = instruction->operands.triple_operands.source_2;
                struct IR_Operand *dest = instruction->operands.triple_operands.destination;
                if(src1->constant && src2->constant) {
                    bool fold_success = true;
                    int64_t src1_val;
                    int64_t src2_val;
                    int64_t res;
                    get_operand_imm_value(src1, &src1_val);
                    get_operand_imm_value(src2, &src2_val);

                    switch (instruction->type) {
                        case IR_INSTRUCTION_TYPE_PLUS:          res = src1_val + src2_val; break;
                        case IR_INSTRUCTION_TYPE_MINUS:         res = src1_val - src2_val; break;
                        case IR_INSTRUCTION_TYPE_DIVIDE:        
                                                                if (src2_val != 0) res = src1_val / src2_val; 
                                                                else fold_success = false;
                                                                break;
                        case IR_INSTRUCTION_TYPE_MOD:           res = src1_val % src2_val; break;
                        case IR_INSTRUCTION_TYPE_MUL:           res = src1_val * src2_val; break;
                        case IR_INSTRUCTION_TYPE_SHL:           res = src1_val << src2_val; break;
                        case IR_INSTRUCTION_TYPE_SHR:           res = src1_val >> src2_val; break;
                        case IR_INSTRUCTION_TYPE_BITWISE_AND:   res = src1_val & src2_val; break;
                        case IR_INSTRUCTION_TYPE_BITWISE_OR:    res = src1_val | src2_val; break;
                        case IR_INSTRUCTION_TYPE_BITWISE_XOR:   res = src1_val ^ src2_val; break;
                        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:   res = src1_val == src2_val; break;
                        case IR_INSTRUCTION_TYPE_GREATER_EQUAL: res = src1_val >= src2_val; break;
                        case IR_INSTRUCTION_TYPE_LESS_EQUAL:    res = src1_val <= src2_val; break;
                        case IR_INSTRUCTION_TYPE_GREATER:       res = src1_val > src2_val; break;
                        case IR_INSTRUCTION_TYPE_LESS:          res = src1_val < src2_val; break;
                        default: break;
                    }

                    if(fold_success) {
                        dest->constant = true;

                        char buffer[20];
                        snprintf(buffer, sizeof(buffer), "%ld", res);

                        remove_instruction_from_use_list(src1, instruction);
                        remove_instruction_from_use_list(src2, instruction);

                        struct IR_Operand *imm_op = IR_create_IR_Operand(context->codegen->arena, IR_OPERAND_TYPE_IMM, instruction, function, dest->in_loop);
                        imm_op->type_info = dest->type_info;
                        imm_op->data.imm_value = str_view_from_cstr(context->codegen->arena, buffer); 

                        instruction->type = IR_INSTRUCTION_TYPE_MOV;
                        instruction->operands.double_operands.source_1 = imm_op;
                        instruction->operands.double_operands.destination = dest;

                        changed = true;
                    }
                }
            }

            instruction = instruction->next;
        }
        block = block->next;
    }


    return changed;
}

static bool replace_operand_in_instruction(struct IR_Instruction *inst, struct IR_Operand *old_op, struct IR_Operand *new_op) {
    if (!inst || !old_op || !new_op) return false;
    bool replaced = false;

    switch (inst->type) {
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE:
        case IR_INSTRUCTION_TYPE_UNARY_NOT:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS:
        case IR_INSTRUCTION_TYPE_LOAD:
        case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_MOV: {

            if (inst->operands.double_operands.source_1 == old_op) {
                inst->operands.double_operands.source_1 = new_op;
                replaced = true;
            }
            break;
        }case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_MOD:
        case IR_INSTRUCTION_TYPE_MUL:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_BITWISE_AND:
        case IR_INSTRUCTION_TYPE_BITWISE_OR:
        case IR_INSTRUCTION_TYPE_BITWISE_XOR:
        case IR_INSTRUCTION_TYPE_SHR:
        case IR_INSTRUCTION_TYPE_SHL:
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_LESS: {
            if (inst->operands.triple_operands.source_1 == old_op) {
                inst->operands.triple_operands.source_1 = new_op;
                replaced = true;
            }
            if (inst->operands.triple_operands.source_2 == old_op) {
                inst->operands.triple_operands.source_2 = new_op;
                replaced = true;
            }
            break;
        }case IR_INSTRUCTION_TYPE_STORE:{
            if (inst->operands.double_operands.destination == old_op) {
                inst->operands.double_operands.destination = new_op;
                replaced = true;
            }
            if (inst->operands.double_operands.source_1 == old_op) {
                inst->operands.double_operands.source_1 = new_op;
                replaced = true;
            }
            break;
        }case IR_INSTRUCTION_TYPE_RET:{
            if (inst->operands.ret.return_value == old_op) {
                inst->operands.ret.return_value = new_op;
                replaced = true;
            }
            break;
        }case IR_INSTRUCTION_TYPE_CALL:{
            if (inst->operands.call.arguments) {
                for (int i = 0; i < inst->operands.call.arguments->element_count; ++i) {
                    struct IR_Operand **arg = vector_get(inst->operands.call.arguments, i);
                    if (*arg == old_op) {
                        *arg = new_op;
                        replaced = true;
                    }
                }
            }
            break;
        }case IR_INSTRUCTION_TYPE_JMP:{
            if (inst->operands.jmp.args) {
                for (int i = 0; i < inst->operands.jmp.args->element_count; ++i) {
                    struct IR_Operand **arg = vector_get(inst->operands.jmp.args, i);
                    if (*arg == old_op) {
                        *arg = new_op;
                        replaced = true;
                    }
                }
            }
            break;
        }case IR_INSTRUCTION_TYPE_BR:{
            if (inst->operands.br.true_args) {
                for (int i = 0; i < inst->operands.br.true_args->element_count; ++i) {
                    struct IR_Operand **arg = vector_get(inst->operands.br.true_args, i);
                    if (*arg == old_op) {
                        *arg = new_op;
                        replaced = true;
                    }
                }
            }
            if (inst->operands.br.false_args) {
                for (int i = 0; i < inst->operands.br.false_args->element_count; ++i) {
                    struct IR_Operand **arg = vector_get(inst->operands.br.false_args, i);
                    if (*arg == old_op) {
                        *arg = new_op;
                        replaced = true;
                    }
                }
            }
            if (inst->operands.br.condition == old_op) {
                inst->operands.br.condition = new_op;
                replaced = true;
            }
            break;
        }case IR_INSTRUCTION_TYPE_ALLOCA:
        case IR_INSTRUCTION_TYPE_NOP:
        case IR_INSTRUCTION_TYPE_UNDEFINED:
        case IR_INSTRUCTION_TYPE_ASM:
        break;
    }

    return replaced;
}

bool opt_copy_propagation(struct opt_context_t *context, struct IR_Function *function) {
    bool changed = false;

    struct IR_Block *block = function->head_block;
    while (NULL != block) {
        struct IR_Instruction *inst = block->head_instruction;

        while (NULL != inst) {
            if (inst->type == IR_INSTRUCTION_TYPE_MOV) {
                struct IR_Operand *dest = inst->operands.double_operands.destination;
                struct IR_Operand *src  = inst->operands.double_operands.source_1;

                if (dest && src && dest->type == IR_OPERAND_TYPE_VREG && src->type == IR_OPERAND_TYPE_VREG && dest != src) {

                    bool is_used_in_branch = false;
                    if (dest->use_list) {
                        for (int i = 0; i < dest->use_list->element_count; ++i) {
                            struct IR_Instruction *user_inst = *(struct IR_Instruction **) vector_get(dest->use_list, i);
                            if (user_inst->type == IR_INSTRUCTION_TYPE_JMP || user_inst->type == IR_INSTRUCTION_TYPE_BR) {
                                is_used_in_branch = true;
                                break;
                            }
                        }
                    }

                    if (!is_used_in_branch) {
                        while (dest->use_list && dest->use_list->element_count > 0) {
                            int last_idx = dest->use_list->element_count - 1;
                            struct IR_Instruction *user_inst = *(struct IR_Instruction **) vector_get(dest->use_list, last_idx);

                            if (replace_operand_in_instruction(user_inst, dest, src)) {
                                vector_add(src->use_list, &user_inst);
                                changed = true;
                            }

                            vector_remove_at(dest->use_list, last_idx);
                        }
                        remove_instruction_from_use_list(src, inst);

                        inst->type = IR_INSTRUCTION_TYPE_NOP;
                        changed = true;
                    }
                }
            }

            inst = inst->next; 
        }

        block = block->next;
    }

    return changed;
}
