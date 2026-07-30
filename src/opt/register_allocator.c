#include "opt/register_allocator.h"
#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

struct register_allocator_t *register_allocator_create_register_allocator(struct codegen_t *codegen) {
    struct register_allocator_t *allocator = calloc(1, sizeof(struct register_allocator_t));
    allocator->codegen = codegen;
    return allocator;
}
void register_allocator_delete_register_allocator(struct register_allocator_t **register_allocator) {
    free(*register_allocator);
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

    for(int i = 0;i < vreg->use_list->element_count; ++i) {
        struct IR_Instruction *instruction = *(struct IR_Instruction **) vector_get(vreg->use_list, i);
        preferred_reg = target->get_fixed_register_for_instruction(target->registers, instruction, vreg);
        if(!preferred_reg || preferred_reg->is_busy) continue;

        if(preferred_reg) return preferred_reg;
    }

    return NULL;
}

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function) {
    if(!function || !allocator) return;
    struct codegen_t *codegen = allocator->codegen;
    struct codegen_build_target_t *target = codegen->current_build_target;
    function->used_callee_saved_registers = bitset_create(target->registers->register_count);

    // calculate weights
    int vreg_count = function->unique_vregs->element_count;
    struct vector_t *vregs = vector_create_vector(vreg_count, sizeof(struct IR_Operand *));

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

    struct vector_t *slots = vector_create_vector(vreg_count/4, sizeof(struct stack_slot_t *));
    for(int i = 0;i < vregs->element_count; ++i) {
        struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(vregs, i);
        int current_time = vreg->data.vreg.live_interval.start;

        register_allocator_expire_old_intervals(target->registers, slots, current_time);


        struct register_t *preferred_reg = register_allocator_check_preferred_reg(vreg, target);

        struct register_t *reg = target->get_best_available_register(target->registers, preferred_reg);
        if(!reg) {
            register_allocator_spill(vreg, slots);
            continue;
        } 
        reg->is_busy = true;
        reg->current_vreg = vreg;
        vreg->data.vreg.reg = reg;

        if(REGISTER_TYPE_CALLEE_SAVED == reg->type) bitset_set(function->used_callee_saved_registers, reg->id);
    }
    vector_free(&vregs);
    vector_free(&slots);
}

struct stack_slot_t *register_allocator_spill(struct IR_Operand *vreg, struct vector_t *stack_slots) {
    if(!vreg || !vreg->definition_instruction) return NULL;
    struct IR_Function *function = vreg->definition_instruction->parent_block->parent_function;
    vreg->type = IR_OPERAND_TYPE_STACK_SLOT;
    vreg->data.slot.live_interval = vreg->data.vreg.live_interval;

    for(int i = 0;i < stack_slots->element_count;++i) {
        struct stack_slot_t *slot = *(struct stack_slot_t **) vector_get(stack_slots, i);
        if(!slot || vreg->type_info->size != slot->type->size) continue;
        if(false == slot->is_busy) {
            vreg->data.slot.stack_slot = slot;
            slot->current_vreg = vreg;
            slot->is_busy = true;
            return slot;
        }
    }

    struct stack_slot_t *slot = IR_create_stack_slot(vreg->type_info, function);
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
    for(int i = 0;i < stack_slots->element_count; ++i) {
        struct stack_slot_t *slot = *(struct stack_slot_t **) vector_get(stack_slots, i);
        if(NULL == slot || NULL == slot->current_vreg) continue;

        if(slot->current_vreg->data.slot.live_interval.end < time) {
            slot->current_vreg = NULL;
            slot->is_busy = false;
        }
    }
}
