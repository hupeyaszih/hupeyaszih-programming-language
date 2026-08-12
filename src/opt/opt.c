#include "opt/opt.h"
#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_bitset.h"
#include "h_vector.h"
#include "opt/register_allocator.h"
#include <stdio.h>

static inline struct vector_t *get_used_operands(struct IR_Instruction *instruction) {
    struct vector_t *list = vector_create_vector(3, sizeof(struct IR_Operand *));

    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_LOAD:
        case IR_INSTRUCTION_TYPE_STORE: {
            vector_add(list, &instruction->operands.double_operands.source_1);
            break;
        }
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_MUL: {
            vector_add(list, &instruction->operands.triple_operands.source_1);
            vector_add(list, &instruction->operands.triple_operands.source_2);
            break;
        }
        case IR_INSTRUCTION_TYPE_BR: {
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
        }
        case IR_INSTRUCTION_TYPE_RET: {
            vector_add(list, &instruction->operands.ret.return_value);
            break;
        }
        case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            vector_add(list, &instruction->operands.double_operands.source_1);
            break;
        }
        case IR_INSTRUCTION_TYPE_CALL: {
           struct vector_t *args = instruction->operands.call.arguments;
           int arg_count = args->element_count;

           for(int i = 0;i < arg_count; ++i) {
               struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
               vector_add(list, &arg);
           }
           break;
        }default: {
            break;
        }
    }

    return list;
}
static inline struct vector_t *get_defined_operands(struct IR_Instruction *instruction) {
    struct vector_t *list = vector_create_vector(3, sizeof(struct IR_Operand *));

    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_LOAD: {
            vector_add(list, &instruction->operands.double_operands.destination);
            break;
        }
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_DIVIDE:
        case IR_INSTRUCTION_TYPE_MUL: {
            vector_add(list, &instruction->operands.triple_operands.destination);
            break;
        }
        case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            vector_add(list, &instruction->operands.double_operands.destination);
            break;
        }
        case IR_INSTRUCTION_TYPE_CALL: {
           vector_add(list, &instruction->operands.call.return_val);
           break;
        }default: {
            break;
        }
    }

    return list;
}

void opt_optimize_project(struct IR_Project *restrict project, struct codegen_t *codegen) {
    if(!project || !codegen) return;

    struct opt_context_t opt_context;
    opt_context.register_allocator = register_allocator_create_register_allocator(codegen);
    opt_context.codegen = codegen;

    for(int i = 0;i < project->modules->element_count; ++i) {
        struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
        opt_optimize_module(&opt_context, module);
    }


    register_allocator_delete_register_allocator(&opt_context.register_allocator);
}

void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module) {
    if(!module) return;

    for(int i = 0;i < module->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **) vector_get(module->functions, i);
        //
        opt_run_cfg_analysis(function);
        opt_compute_use_def(function);
        opt_run_live_range_analysis(function, context);
        register_allocator_run_allocator(context->register_allocator, function);
        //
    }

    register_allocator_compute_caller_saved_registers(context->codegen->current_build_target, module->parent_project->main_function);

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

void opt_compute_use_def(struct IR_Function *function) {
    int vreg_count = function->unique_vregs->element_count;

    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        block->use       = bitset_create(vreg_count);
        block->def       = bitset_create(vreg_count);

        while(NULL != instruction) {
            switch (instruction->type) {
                case IR_INSTRUCTION_TYPE_MOV:
                case IR_INSTRUCTION_TYPE_LOAD: {
                    process_use(instruction->operands.double_operands.source_1, block->use, instruction);
                    process_def(instruction->operands.double_operands.destination, block->def);
                    break;
                }
                case IR_INSTRUCTION_TYPE_STORE: {
                    process_use(instruction->operands.double_operands.source_1, block->use, instruction);
                    break;
                }
                case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
                case IR_INSTRUCTION_TYPE_BANG_EQUAL:
                case IR_INSTRUCTION_TYPE_LESS_EQUAL:
                case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
                case IR_INSTRUCTION_TYPE_LESS:
                case IR_INSTRUCTION_TYPE_GREATER:
                case IR_INSTRUCTION_TYPE_PLUS:
                case IR_INSTRUCTION_TYPE_MINUS:
                case IR_INSTRUCTION_TYPE_DIVIDE:
                case IR_INSTRUCTION_TYPE_MUL: {
                    process_use(instruction->operands.triple_operands.source_1, block->use, instruction);
                    process_use(instruction->operands.triple_operands.source_2, block->use, instruction);
                    process_def(instruction->operands.triple_operands.destination, block->def);
                    break;
                }
                case IR_INSTRUCTION_TYPE_BR: {
                    process_use(instruction->operands.br.condition, block->use, instruction);

                    struct vector_t *false_args = instruction->operands.br.false_args;
                    if(false_args) {
                        for(int i = 0; i < false_args->element_count; ++i) {
                            struct IR_Operand *arg = *(struct IR_Operand **) vector_get(false_args, i);
                            process_use(arg, block->use, instruction);
                        }
                    }

                    struct vector_t *true_args = instruction->operands.br.true_args;
                    if(true_args) {
                        for(int i = 0; i < true_args->element_count; ++i) {
                            struct IR_Operand *arg = *(struct IR_Operand **) vector_get(true_args, i);
                            process_use(arg, block->use, instruction);
                        }
                    }

                    break;
                }case IR_INSTRUCTION_TYPE_JMP: {
                    if(!instruction->operands.jmp.args) break;
                    struct vector_t *args = instruction->operands.jmp.args;
                    for(int i = 0; i < args->element_count; ++i) {
                        struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
                        process_use(arg, block->use, instruction);
                    }
                    break;
                }case IR_INSTRUCTION_TYPE_RET: {
                    process_use(instruction->operands.ret.return_value, block->use, instruction);
                    break;
                }
                case IR_INSTRUCTION_TYPE_CAST:
                case IR_INSTRUCTION_TYPE_UNARY_BANG:
                case IR_INSTRUCTION_TYPE_UNARY_MINUS: 
                case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
                case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
                    process_use(instruction->operands.double_operands.source_1, block->use, instruction);
                    process_def(instruction->operands.double_operands.destination, block->def);
                    break;
                }
                case IR_INSTRUCTION_TYPE_CALL: {
                    struct vector_t *args = instruction->operands.call.arguments;
                    int arg_count = args->element_count;

                    for(int i = 0;i < arg_count; ++i) {
                        struct IR_Operand *arg = *(struct IR_Operand **) vector_get(args, i);
                        process_use(arg, block->use, instruction);
                    }

                    process_def(instruction->operands.call.return_val, block->def);
                    break;
                }default: {
                    break;
                }
            }

            instruction = instruction->next;
        }
        block = block->next;
    }
}

void opt_run_live_range_analysis(struct IR_Function *function, struct opt_context_t *context) {
    for(int i = 0; i < function->parameters->element_count; ++i) {
        struct IR_Operand *param = *(struct IR_Operand **) vector_get(function->parameters, i);
        if (param && IR_OPERAND_TYPE_VREG == param->type) {
            param->data.vreg.live_interval.start = 0;
        }
    }

    struct IR_Block *block = function->tail_block;
    while(NULL != block) {

        if (NULL == block->head_instruction) {
            block = block->prev;
            continue;
        }

        struct IR_Instruction *instruction = block->tail_instruction;
        while(NULL != instruction) {

            struct vector_t *used_operands = get_used_operands(instruction);
            struct vector_t *defined_operands = get_defined_operands(instruction);

            for(int i = 0; i < used_operands->element_count; ++i) {
                struct IR_Operand *operand = *(struct IR_Operand **) vector_get(used_operands, i);
                if(!operand || IR_OPERAND_TYPE_VREG != operand->type) continue;
                operand->data.vreg.live_interval.use_score += block->in_loop*10+1;

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

            vector_free(&used_operands);
            vector_free(&defined_operands);


            instruction = instruction->prev;
        }

        int block_start_id = block->head_instruction->id;
        int block_end_id = block->tail_instruction ? block->tail_instruction->id : block_start_id;

        for(int i = 0; i < block->params->element_count; ++i) {
            struct IR_Operand *param = *(struct IR_Operand **) vector_get(block->params, i);
            if(!param || IR_OPERAND_TYPE_VREG != param->type) continue;

            param->data.vreg.live_interval.start = block_start_id;
            if (param->data.vreg.live_interval.end <= block_start_id) {
                param->data.vreg.live_interval.end = block_end_id + 1;
            }
        }

        block = block->prev;
    }

}
