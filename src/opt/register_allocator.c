#include "opt/register_allocator.h"
#include "backend/codegen.h"
#include "backend/x86_64/x86_64_linux.h"
#include "core/ir_gen.h"
#include "h_arena.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct graph_node {
    struct IR_Operand *vreg;
    struct bitset_t *edges;
    struct bitset_t *clobbers;
    struct bitset_t *preferred_regs;
    int pre_colored_reg;
    int degree;
    int vreg_id;
    bool is_spilled;
    bool is_simplified;
};


struct register_allocator_t *register_allocator_create_register_allocator(struct codegen_t *codegen) {
    struct register_allocator_t *allocator = arena_alloc(codegen->arena, sizeof(struct register_allocator_t));
    allocator->codegen = codegen;
    return allocator;
}

void register_allocator_check_preferred_reg(struct graph_node *node, struct codegen_build_target_t *target) {
    target->get_preferred_registers(target->registers, node->vreg, node->preferred_regs);
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

static void register_allocator_add_to_call_across_registers(struct IR_Function *function,struct arena *arena,struct codegen_build_target_t *target,struct IR_Operand *vreg,struct register_t *reg) {
    int start = vreg->data.vreg.live_interval.start;
    int end   = vreg->data.vreg.live_interval.end;

    struct IR_Block *block = function->head_block;
    while (NULL != block) {
        struct IR_Instruction *instruction = block->head_instruction;
        while (NULL != instruction) {

            if (IR_INSTRUCTION_TYPE_CALL == instruction->type && instruction->id >= start && instruction->id <= end) {
                
                if (!instruction->operands.call.across_registers) {
                    instruction->operands.call.across_registers = bitset_create(arena, target->registers->register_count);
                }
                bitset_set(instruction->operands.call.across_registers, reg->id);
            }
            instruction = instruction->next;
        }
        block = block->next;
    }
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

static inline bool intersects(struct IR_Operand *op1, struct IR_Operand *op2) {
    int start_1 = op1->data.vreg.live_interval.start;
    int start_2 = op2->data.vreg.live_interval.start;

    int end_1 = op1->data.vreg.live_interval.end;
    int end_2 = op2->data.vreg.live_interval.end;

    if (start_1 == -1 || end_1 == -1 || start_2 == -1 || end_2 == -1) {
        return false;
    }

    return (start_1 <= end_2) && (start_2 <= end_1);
}


static inline void decrease_neighbours_degree(struct graph_node *node, struct vector_t *nodes) {
    int node_count = nodes->element_count;
    for(int k = 0;k < node_count;++k) {
        struct graph_node *target_node = (struct graph_node *) vector_get(nodes, k);
        struct IR_Operand *target_vreg = target_node->vreg;
        int target_vreg_id = target_vreg->data.vreg.vreg_id;

        if(bitset_test(node->edges, target_vreg_id)) {
            --target_node->degree;
        }
    }
}

static inline bool is_register_using_by_neighbours(struct graph_node *node, struct vector_t *nodes, int register_id) {
    int node_count = nodes->element_count;
    bool result = false;
    for(int k = 0;k < node_count; ++k) {
        struct graph_node *target_node = (struct graph_node *) vector_get(nodes, k);
        if(node->vreg->data.vreg.vreg_id == target_node->vreg->data.vreg.vreg_id) continue;
        if(!bitset_test(node->edges, target_node->vreg->data.vreg.vreg_id)) continue;

        if(target_node->vreg->data.vreg.reg && target_node->vreg->data.vreg.reg->id == register_id) {
            result = true;
            break;
        }
    }
    return result;
}

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function) {
    if(!function || !allocator) return;
    struct codegen_t *codegen = allocator->codegen;
    struct codegen_build_target_t *target = codegen->current_build_target;
    struct arena *temp_arena = allocator->codegen->temp_arena;
    struct arena *arena = allocator->codegen->arena;

    function->used_callee_saved_registers          = bitset_create(allocator->codegen->arena, target->registers->register_count);
    function->used_caller_saved_registers          = bitset_create(allocator->codegen->arena, target->registers->register_count);
    function->directly_used_caller_saved_registers = bitset_create(allocator->codegen->arena, target->registers->register_count);

    struct vector_t *clobbers = vector_create_vector(allocator->codegen->temp_arena, function->unique_vregs->element_count / 8+1, sizeof(struct clobber_t));
    calculate_clobbers(function, target, clobbers);

    //
    struct vector_t *vregs = function->unique_vregs;
    int vreg_count = vregs->element_count;
    int phys_reg_count = codegen->current_build_target->registers->register_count;
    if(vreg_count <= 0) return;
    struct vector_t *nodes = vector_create_vector(temp_arena, (vreg_count), sizeof(struct graph_node));
    struct vector_t *slots = vector_create_vector(temp_arena, (vreg_count/4)+1, sizeof(struct stack_slot_t *));


    for(int i = 0;i < vreg_count; ++i) {
        struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(vregs, i);
        if(IR_OPERAND_TYPE_UNDEFINED == vreg->type) {
            continue;
        }
        struct graph_node node;
        node.vreg = vreg;
        node.vreg_id = vreg->data.vreg.vreg_id;
        node.degree = 0;
        node.edges    = bitset_create(temp_arena, vreg_count);
        node.clobbers = bitset_create(temp_arena, phys_reg_count);
        node.preferred_regs = bitset_create(temp_arena, phys_reg_count);
        node.pre_colored_reg = -1;
        node.is_spilled = false;
        node.is_simplified = false;


        for(int k = 0;k < vreg_count; ++k) {
            struct IR_Operand *target_vreg = *(struct IR_Operand **) vector_get(vregs, k);
            if(IR_OPERAND_TYPE_VREG != target_vreg->type) {
                continue;
            }
            if(target_vreg->data.vreg.vreg_id == vreg->data.vreg.vreg_id) continue;
            if(!intersects(vreg, target_vreg)) continue;
            bool is_mov_source = false;
            struct IR_Instruction *def_inst = vreg->definition_instruction;
            if (def_inst && def_inst->type == IR_INSTRUCTION_TYPE_MOV) {
                struct IR_Operand *src = def_inst->operands.double_operands.source_1;
                if (src && src->type == IR_OPERAND_TYPE_VREG && src->data.vreg.vreg_id == target_vreg->data.vreg.vreg_id) {
                    is_mov_source = true; 
                }
            }

            if (is_mov_source) {
                continue;
            }
            bitset_set(node.edges, target_vreg->data.vreg.vreg_id);
            ++node.degree;
        }

        register_allocator_check_preferred_reg(&node, target);

        for(int k = 0;k < phys_reg_count;++k) {
            struct register_t *reg = target->registers->registers + k;
            if(codegen_is_register_clobbered_for_vreg(reg, vreg, clobbers)) {
                bitset_set(node.clobbers, k);
            }
        }

        vector_add(nodes, &node);
    }

    for(int i = 0; i < function->parameters->element_count; ++i) {
        struct IR_Operand *param_vreg = *(struct IR_Operand **) vector_get(function->parameters, i);
        if(IR_OPERAND_TYPE_UNDEFINED == param_vreg->type) continue;

        if (i >= target->argument_register_count) {
            for(int k = 0; k < nodes->element_count; ++k) {
                struct graph_node *node = (struct graph_node *) vector_get(nodes, k);
                if(node->vreg->data.vreg.vreg_id == param_vreg->data.vreg.vreg_id) {
                    register_allocator_spill(allocator->codegen->arena, param_vreg, slots, function, true, node, nodes);
                    decrease_neighbours_degree(node, nodes);
                    node->is_simplified = true;
                    node->is_spilled = true;
                    break;
                }
            }

            continue;
        }

        for(int k = 0; k < nodes->element_count; ++k) {
            struct graph_node *node = (struct graph_node *) vector_get(nodes, k);
            if(node->vreg->data.vreg.vreg_id == param_vreg->data.vreg.vreg_id) {
                struct register_t *reg = target->get_reg_with_arg_index(target->registers, i);
                node->pre_colored_reg = reg->id;
                node->vreg->data.vreg.reg = reg;
                decrease_neighbours_degree(node, nodes);
                node->is_simplified = true;
                break;
            }
        }
    }

    struct vector_t *simplifying_nodes = vector_create_vector(temp_arena, vreg_count, sizeof(struct graph_node *));
    // simplifying
    for(;;) {
        bool made_progress = false;
        for(int i = 0;i < nodes->element_count; ++i) {
            struct graph_node *node = (struct graph_node *) vector_get(nodes, i);
            if(node->is_simplified || node->is_spilled) continue;
            if(node->degree < phys_reg_count) {
                vector_push(simplifying_nodes, &node);

                decrease_neighbours_degree(node, nodes);

                made_progress = true;
                node->is_simplified = true;
            }
        }

        if(made_progress) continue;

        struct graph_node *node_to_spill = NULL;
        for(int i = 0;i < nodes->element_count; ++i) {
            struct graph_node *node = (struct graph_node *) vector_get(nodes, i);
            if(node->is_simplified || node->is_spilled) continue;
            if(NULL == node_to_spill) {
                node_to_spill = node;
                continue;
            }
            if(node->vreg->data.vreg.live_interval.weight < node_to_spill->vreg->data.vreg.live_interval.weight) {
                node_to_spill = node;
            }
        }

        if(node_to_spill) {
            node_to_spill->is_simplified = true;
            vector_push(simplifying_nodes, &node_to_spill);
            decrease_neighbours_degree(node_to_spill, nodes);
        } else {
            break;
        }
    }

    // coloring
    for(int i = 0;i < simplifying_nodes->element_count; ++i) {
        struct graph_node *node = *(struct graph_node **) vector_get(simplifying_nodes, i);
        if(!node || !node->vreg || (-1 == node->pre_colored_reg && bitset_is_zero(node->preferred_regs))) continue;

        if(-1 != node->pre_colored_reg) {
            struct register_t *reg = target->registers->registers + node->pre_colored_reg;
            if(!is_register_using_by_neighbours(node, nodes, reg->id) && !bitset_test(node->clobbers, reg->id)) {
                node->vreg->data.vreg.reg = reg;
            }
        }else if(!bitset_is_zero(node->preferred_regs)){
            for(int k = 0;k < node->preferred_regs->max_element_count; ++k) {
                if(!bitset_test(node->preferred_regs, k)) continue;
                
                struct register_t *reg = target->registers->registers + k;
                if(!is_register_using_by_neighbours(node, nodes, reg->id) && !bitset_test(node->clobbers, reg->id)) {
                    node->vreg->data.vreg.reg = reg;
                    break;
                }
            }
        }
    }

    for(;;) {
        if(simplifying_nodes->element_count <= 0) break;
        struct graph_node *node = *(struct graph_node **) vector_pop(simplifying_nodes);
        if(node->vreg->data.vreg.reg) continue;

        if(-1 != node->pre_colored_reg) {
            struct register_t *reg = target->registers->registers + node->pre_colored_reg;
            if(!is_register_using_by_neighbours(node, nodes, reg->id) && !bitset_test(node->clobbers, reg->id)) {
                node->vreg->data.vreg.reg = reg;
                continue;
            }
        }

        bool assigned = false;
        for(int i = 0;i < phys_reg_count;++i) {
            struct register_t *reg = target->registers->registers + target->get_reg_in_reg_preference_order(i);
            if(!reg) continue;
            if(is_register_using_by_neighbours(node, nodes, reg->id)) continue;
            if(bitset_test(node->clobbers, reg->id)) continue;

            node->vreg->data.vreg.reg = reg;
            assigned = true;
            break;
        }
        if(!assigned) {
            register_allocator_spill(arena, node->vreg, slots, function, false, node,nodes);
            node->is_spilled = true;
        }
    }

    for(int i = 0;i < function->unique_vregs->element_count;++i) {
        struct IR_Operand *vreg = *(struct IR_Operand **) vector_get(function->unique_vregs, i);
        if(!vreg || IR_OPERAND_TYPE_VREG != vreg->type) continue;
        struct register_t *reg = vreg->data.vreg.reg;
        if(!reg) continue;
        bool crosses = register_allocator_vreg_crosses_call(function, vreg);
        vreg->data.vreg.crosses_call = crosses;

        if(REGISTER_TYPE_CALLEE_SAVED == reg->type) bitset_set(function->used_callee_saved_registers, reg->id);
        if(REGISTER_TYPE_CALLER_SAVED == reg->type) bitset_set(function->directly_used_caller_saved_registers, reg->id);

        if(crosses && REGISTER_TYPE_CALLER_SAVED == reg->type) {
            register_allocator_add_to_call_across_registers(function, allocator->codegen->arena, target, vreg, reg);
        }
    }


    arena_reset(temp_arena);
}

struct stack_slot_t *register_allocator_spill(struct arena *arena, struct IR_Operand *vreg, struct vector_t *stack_slots, struct IR_Function *function, bool is_argument,struct graph_node *current_node,struct vector_t *nodes) {
    if (!vreg) return NULL;

    if (current_node && nodes) {
        for (int i = 0; i < stack_slots->element_count; ++i) {
            struct stack_slot_t *slot = *(struct stack_slot_t **) vector_get(stack_slots, i);
            if (!slot || vreg->type_info->size != slot->type->size) continue;
            if (is_argument != slot->is_argument) continue;

            bool interferes = false;

            for (int k = 0; k < nodes->element_count; ++k) {
                struct graph_node *other = (struct graph_node *) vector_get(nodes, k);
                if (other == current_node || !other->is_spilled) continue;

                if (other->vreg->type == IR_OPERAND_TYPE_STACK_SLOT && 
                    other->vreg->data.slot.stack_slot == slot) {
                    
                    int other_vreg_id = other->vreg_id;
                    if (bitset_test(current_node->edges, other_vreg_id)) {
                        interferes = true;
                        break;
                    }
                }
            }

            if (!interferes) {
                vreg->type = IR_OPERAND_TYPE_STACK_SLOT;
                vreg->data.slot.live_interval = vreg->data.vreg.live_interval;
                vreg->data.slot.stack_slot = slot;
                return slot;
            }
        }
    }

    struct stack_slot_t *slot = IR_create_stack_slot(arena, vreg->type_info, function, is_argument);
    vreg->type = IR_OPERAND_TYPE_STACK_SLOT;
    vreg->data.slot.live_interval = vreg->data.vreg.live_interval;
    vreg->data.slot.stack_slot = slot;

    vector_add(stack_slots, &slot);
    return slot;
}
