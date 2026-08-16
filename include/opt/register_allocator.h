#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "backend/codegen.h"
#include "core/ir_gen.h"
struct register_allocator_t {
    struct codegen_t *codegen;
};


void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function);
void register_allocator_compute_caller_saved_registers(struct arena *arena, struct codegen_build_target_t *target, struct IR_Function  *current);

struct register_allocator_t *register_allocator_create_register_allocator(struct codegen_t *codegen);

int register_allocator_compare_operands_by_weight(const void *a, const void *b);
int register_allocator_compare_operands_by_live_interval(const void *a, const void *b);

void register_allocator_expire_old_intervals(struct register_list_t *registers, struct vector_t *stack_slots,int time);
struct register_t *register_allocator_check_preferred_reg(struct IR_Operand *vreg, struct codegen_build_target_t *target);
void register_allocator_emit_movs_for_fixed_regs(struct IR_Operand *vreg, struct codegen_build_target_t *target);

struct graph_node;
struct stack_slot_t *register_allocator_spill(struct arena *arena, struct IR_Operand *vreg, struct vector_t *stack_slots, struct IR_Function *function, bool is_argument,struct graph_node *current_node,struct vector_t *nodes);

#endif
