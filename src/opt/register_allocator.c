#include "opt/register_allocator.h"
#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_arena.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdlib.h>

struct register_allocator_t *register_allocator_create_register_allocator(struct codegen_t *codegen) {
    struct register_allocator_t *allocator = arena_alloc(codegen->arena, sizeof(struct register_allocator_t));
    allocator->codegen = codegen;
    return allocator;
}

int register_allocator_compare_operands_by_weight(const void *a, const void *b) {
    const struct IR_Operand *op_a = *(const struct IR_Operand **)a;
    const struct IR_Operand *op_b = *(const struct IR_Operand **)b;

    if (op_b->data.vreg.live_interval.weight > op_a->data.vreg.live_interval.weight) return 1;
    if (op_b->data.vreg.live_interval.weight < op_a->data.vreg.live_interval.weight) return -1;

    if (op_a->data.vreg.vreg_id > op_b->data.vreg.vreg_id) return 1;
    if (op_a->data.vreg.vreg_id < op_b->data.vreg.vreg_id) return -1;
    return 0;
}

int register_allocator_compare_operands_by_live_interval(const void *a, const void *b) {
    const struct IR_Operand *op_a = *(const struct IR_Operand **)a;
    const struct IR_Operand *op_b = *(const struct IR_Operand **)b;

    if (op_a->data.vreg.live_interval.start != op_b->data.vreg.live_interval.start) {
        return op_a->data.vreg.live_interval.start - op_b->data.vreg.live_interval.start;
    }

    return op_a->data.vreg.live_interval.end - op_b->data.vreg.live_interval.end;

    return 0;
}

struct register_t *register_allocator_check_preferred_reg(struct IR_Operand *vreg, struct codegen_build_target_t *target) {
    struct register_t *preferred_reg = NULL;

    if(vreg->definition_instruction){
        struct IR_Instruction *instruction = vreg->definition_instruction;
        preferred_reg = target->get_fixed_register_for_instruction(target->registers, instruction, vreg);
        if(preferred_reg && !preferred_reg->is_busy){
            return preferred_reg;
        }

    }

    for(int i = 0;i < vreg->use_list->element_count; ++i) {
        struct IR_Instruction *instruction = *(struct IR_Instruction **) vector_get(vreg->use_list, i);
        preferred_reg = target->get_fixed_register_for_instruction(target->registers, instruction, vreg);
        if(!preferred_reg || preferred_reg->is_busy) continue;

        return preferred_reg;
    }

    return NULL;
}

bool register_allocator_vreg_crosses_call(struct IR_Function *function, struct IR_Operand *vreg) {
    int start = vreg->data.vreg.live_interval.start;
    int end = vreg->data.vreg.live_interval.end;

    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        while(NULL != instruction) {
            if(IR_INSTRUCTION_TYPE_CALL == instruction->type && instruction->id >= start && instruction->id <= end) return true;
            instruction = instruction->next;
        }
        block = block->next;
    }
    return false;
}


void register_allocator_compute_caller_saved_registers(struct arena *arena, struct codegen_build_target_t *target, struct IR_Function  *current) {
    if (!current) return;
    if (current->is_visiting) return;
    if (current->is_fully_processed) return;
    current->is_visiting = true;

    if (!current->used_caller_saved_registers) {
        current->used_caller_saved_registers = bitset_create(arena, target->registers->register_count);
    }

    if (current->directly_used_caller_saved_registers) {
        bitset_or(current->used_caller_saved_registers, current->directly_used_caller_saved_registers);
    }

    struct IR_Block *block = current->head_block;
    while (block != NULL) {
        struct IR_Instruction *instruction = block->head_instruction;
        while (instruction != NULL) {

            if (instruction->type == IR_INSTRUCTION_TYPE_CALL) {
                struct IR_Function *callee = instruction->operands.call.target_function;

                if (callee) {
                    register_allocator_compute_caller_saved_registers(arena, target, callee);

                    if (callee->used_caller_saved_registers) {
                        bitset_or(current->used_caller_saved_registers, 
                                  callee->used_caller_saved_registers);
                    }
                } else {
                    for(int i = 0;i < target->registers->register_count; ++i) {
                        struct register_t *reg = target->registers->registers + i;
                        if(REGISTER_TYPE_CALLER_SAVED != reg->type) continue;
                        bitset_set(current->used_caller_saved_registers, i);
                    }
                }
            }

            instruction = instruction->next;
        }
        block = block->next;
    }

    current->is_visiting = false;
    current->is_fully_processed = true;
}

static inline void calculate_clobbers(struct IR_Function *function, struct codegen_build_target_t *target, struct vector_t *clobbers) {
    int time = 0;
    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        while(NULL != instruction) {
            time = instruction->id;

            target->collect_instruction_clobbers(target->registers, instruction, clobbers, time);

            instruction = instruction->next;
        }
        block = block->next;
    }
}

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function) {
    if(!function || !allocator) return;
    struct codegen_t *codegen = allocator->codegen;
    struct codegen_build_target_t *target = codegen->current_build_target;

    for(int i = 0;i < target->registers->register_count; ++i) {
        struct register_t *reg = target->registers->registers + i;
        reg->is_busy = false;
        reg->current_vreg = NULL;
    }


    function->used_callee_saved_registers          = bitset_create(allocator->codegen->arena, target->registers->register_count);
    function->used_caller_saved_registers          = bitset_create(allocator->codegen->arena, target->registers->register_count);
    function->directly_used_caller_saved_registers = bitset_create(allocator->codegen->arena, target->registers->register_count);

    struct vector_t *clobbers = vector_create_vector(allocator->codegen->temp_arena, function->unique_vregs->element_count / 8+1, sizeof(struct clobber_t));
    calculate_clobbers(function, target, clobbers);

    // calculate weights
    int vreg_count = function->unique_vregs->element_count;
    if(vreg_count <= 0) return;
    struct vector_t *vregs = vector_create_vector(allocator->codegen->temp_arena, vreg_count, sizeof(struct IR_Operand *));
    struct vector_t *slots = vector_create_vector(allocator->codegen->temp_arena, (vreg_count/4)+1, sizeof(struct stack_slot_t *));


    for(int i = 0;i < vreg_count; ++i) {
        struct IR_Operand *operand = *(struct IR_Operand **) vector_get(function->unique_vregs, i);
        int use_score = operand->data.vreg.live_interval.use_score;
        int live_length = operand->data.vreg.live_interval.end - operand->data.vreg.live_interval.start;

        int weight = 0;
        if(live_length == 0) {
            weight = 1;
        }else {
            weight = (use_score << 10) / live_length;
        }

        operand->data.vreg.live_interval.weight = weight;

        vector_add(vregs, &operand);
    }

    qsort(vregs->data, vregs->element_count, vregs->type_size, &register_allocator_compare_operands_by_live_interval);
    // Register Allocator

    bool *is_allocated = arena_alloc(codegen->temp_arena, vreg_count * sizeof(bool));
    int parameter_count = function->parameters->element_count;
    for(int i = 0; i < parameter_count; ++i) {
        struct IR_Operand *param = *(struct IR_Operand **) vector_get(function->parameters, i);
        is_allocated[param->data.vreg.vreg_id] = true;

        if (i >= target->argument_register_count) {
            register_allocator_spill(allocator->codegen->arena, param, slots, function, true);
            continue;
        }

        struct register_t *reg = target->get_reg_with_arg_index(target->registers, i);
        reg->is_busy = true;
        reg->current_vreg = param;
        param->data.vreg.reg = reg;

        if(REGISTER_TYPE_CALLEE_SAVED == reg->type) bitset_set(function->used_callee_saved_registers, reg->id);
        if(REGISTER_TYPE_CALLER_SAVED == reg->type) bitset_set(function->directly_used_caller_saved_registers, reg->id);

        if(register_allocator_vreg_crosses_call(function, param)) {
            param->data.vreg.crosses_call = true;
        }
    }

    for(int i = 0;i < vregs->element_count; ++i) {
        struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(vregs, i);
        if(is_allocated[vreg->data.vreg.vreg_id]) continue;
        int current_time = vreg->data.vreg.live_interval.start;

        register_allocator_expire_old_intervals(target->registers, slots, current_time);

        is_allocated[vreg->data.vreg.vreg_id] = true;
        struct register_t *preferred_reg = register_allocator_check_preferred_reg(vreg, target);

        struct register_t *reg = target->get_best_available_register(target->registers, preferred_reg, clobbers, vreg);
        if(!reg) {
            register_allocator_spill(allocator->codegen->arena, vreg, slots, function, false);
            continue;
        } 
        reg->is_busy = true;
        reg->current_vreg = vreg;
        vreg->data.vreg.reg = reg;

        if(REGISTER_TYPE_CALLEE_SAVED == reg->type) bitset_set(function->used_callee_saved_registers, reg->id);
        if(REGISTER_TYPE_CALLER_SAVED == reg->type) bitset_set(function->directly_used_caller_saved_registers, reg->id);

        if(register_allocator_vreg_crosses_call(function, vreg)) {
            vreg->data.vreg.crosses_call = true;
        }
    }

}

struct stack_slot_t *register_allocator_spill(struct arena *arena, struct IR_Operand *vreg, struct vector_t *stack_slots, struct IR_Function *function, bool is_argument) {
    if(!vreg || !vreg) return NULL;
    vreg->type = IR_OPERAND_TYPE_STACK_SLOT;
    vreg->data.slot.live_interval = vreg->data.vreg.live_interval;

    for(int i = 0;i < stack_slots->element_count;++i) {
        struct stack_slot_t *slot = *(struct stack_slot_t **) vector_get(stack_slots, i);
        if(!slot || vreg->type_info->size != slot->type->size) continue;
        if(false == slot->is_busy && is_argument == slot->is_argument) {
            vreg->data.slot.stack_slot = slot;
            slot->current_vreg = vreg;
            slot->is_busy = true;
            return slot;
        }
    }

    struct stack_slot_t *slot = IR_create_stack_slot(arena, vreg->type_info, function, is_argument);
    slot->current_vreg = vreg;
    vreg->data.slot.stack_slot = slot;
    slot->is_busy = true;

    vector_add(stack_slots, &slot);
    return slot;
}

void register_allocator_expire_old_intervals(struct register_list_t *registers, struct vector_t *stack_slots,int time) {
    for(int i = 0;i < registers->register_count; ++i) {
        struct register_t *reg = registers->registers + i;
        if(!reg->is_busy || !reg->current_vreg) continue;

        if(reg->current_vreg->data.vreg.live_interval.end < time) {
            reg->current_vreg = NULL;
            reg->is_busy = false;
        }
    }
    if(!stack_slots) return;
    for(int i = 0;i < stack_slots->element_count; ++i) {
        struct stack_slot_t *slot = *(struct stack_slot_t **) vector_get(stack_slots, i);
        if(NULL == slot || NULL == slot->current_vreg) continue;

        if(slot->current_vreg->data.slot.live_interval.end < time) {
            slot->current_vreg = NULL;
            slot->is_busy = false;
        }
    }
}
