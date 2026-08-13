#include "core/ir_gen.h"
#include "core/symbol_table.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// create/free

struct stack_slot_t *IR_create_stack_slot(struct type_info *type, struct IR_Function *function, bool is_argument) {
    struct stack_slot_t *slot = calloc(1, sizeof(struct stack_slot_t));
    slot->type = type;
    slot->current_vreg = NULL;
    slot->is_busy = false;
    slot->is_argument = is_argument;

    if(!is_argument) {
        slot->stack_offset = function->stack_size;
        function->stack_size += type_table_size_padding(type->size);
    }else {
        slot->stack_offset = function->stack_size_for_args;
        function->stack_size_for_args += type_table_size_padding(type->size);
    }

    vector_add(function->stack_slots, &slot);
    return slot;
}
void IR_delete_stack_slot(struct stack_slot_t **slot) {
    free((*slot));
    (*slot) = NULL;
}

struct IR_Project *IR_create_IR_Project() {
    struct IR_Project *project = calloc(1, sizeof(struct IR_Project));
    project->modules = vector_create_vector(1, sizeof(struct IR_Module *));

    return project;
}
void IR_delete_IR_Project(struct IR_Project **project) {
    if(!project || !(*project)) return;
    for(int i = 0;i < (*project)->modules->element_count; ++i) {
        struct IR_Module *module = *(struct IR_Module **)vector_get((*project)->modules, i);
        IR_delete_IR_Module(&module);
    }

    vector_free(&(*project)->modules);
    free((*project));
    (*project) = NULL;
}

struct IR_Module *IR_create_IR_Module(char *name) {
    struct IR_Module *module = calloc(1, sizeof(struct IR_Module));
    module->functions = vector_create_vector(1, sizeof(struct IR_Function *));
    module->name = name;

    return module;
}
void IR_delete_IR_Module(struct IR_Module **module) {
    for(int i = 0;i < (*module)->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **)vector_get((*module)->functions, i);
        IR_delete_IR_Function(&function);
    }

    vector_free(&(*module)->functions);
    free((*module));
    (*module) = NULL;
}

struct IR_Function *IR_create_IR_Function(struct str_view name, struct str_view mangled_name, int parameter_count) {
    struct IR_Function *function = calloc(1, sizeof(struct IR_Function));
    function->name = name;
    function->mangled_name = mangled_name;

    function->parameters = vector_create_vector(6, sizeof(struct IR_Operand *));
    function->operands = vector_create_vector(16, sizeof(struct IR_Operand *));
    function->stack_slots = vector_create_vector(8, sizeof(struct stack_slot_t *));
    function->unique_vregs = vector_create_vector(16, sizeof(struct IR_Operand *));
    function->used_callee_saved_registers = NULL;

    function->parameter_count = parameter_count;
    function->instruction_count = 0;
    function->stack_size = 8;
    function->stack_size_for_args = 0;
    function->vreg_counter = 0;

    function->is_fully_processed = false;
    function->is_visiting = false;
    return function;
}
void IR_delete_IR_Function(struct IR_Function **function) {
    if (function == NULL || *function == NULL) return;

    struct IR_Block *block = (*function)->head_block;
    while (block != NULL) {
        struct IR_Block *next_block = block->next;
        IR_delete_IR_Block(&block);
        block = next_block;
    }

    for(int i = 0; i < (*function)->operands->element_count; ++i) {
        struct IR_Operand *operand = *(struct IR_Operand **)vector_get((*function)->operands, i);
        IR_delete_IR_Operand(&operand);
    }

    for(int i = 0; i < (*function)->stack_slots->element_count; ++i) {
        struct stack_slot_t *slot = *(struct stack_slot_t **)vector_get((*function)->stack_slots, i);
        IR_delete_stack_slot(&slot);
    }

    vector_free(&(*function)->unique_vregs);
    vector_free(&(*function)->operands);
    vector_free(&(*function)->parameters);
    vector_free(&(*function)->stack_slots);

    bitset_free(&(*function)->used_callee_saved_registers);
    bitset_free(&(*function)->used_caller_saved_registers);
    bitset_free(&(*function)->directly_used_caller_saved_registers);

    free((*function));
    (*function) = NULL;
}

struct IR_Block *IR_create_IR_Block(struct IR_Function *parent_function, struct str_view mangled_name) {
    struct IR_Block *block = calloc(1, sizeof(struct IR_Block));
    block->mangled_name = mangled_name;
    block->parent_function = parent_function;
    block->predecessor = vector_create_vector(1, sizeof(struct IR_Block *));
    block->successors = vector_create_vector(1, sizeof(struct IR_Block *));

    block->use = NULL;
    block->def = NULL;

    block->head_instruction = NULL;
    block->tail_instruction = NULL;
    block->loop_tail_instruction = NULL;
    block->loop_head_instruction = NULL;
    block->next = NULL;
    block->prev = NULL;

    block->instruction_count = 0;
    block->in_loop = 0;

    block->params = vector_create_vector(2, sizeof(struct IR_Operand *));

    return block;
}
void IR_delete_IR_Block(struct IR_Block **block) {
    struct IR_Instruction *instruction = (*block)->head_instruction;
    while(instruction != NULL) {
        struct IR_Instruction *next_instruction = instruction->next;
        IR_delete_IR_Instruction(&instruction);
        instruction = next_instruction;
    }

    vector_free(&(*block)->predecessor);
    vector_free(&(*block)->successors);
    vector_free(&(*block)->params);

    bitset_free(&(*block)->use);
    bitset_free(&(*block)->def);

    free((*block));
    (*block) = NULL;
}

struct IR_Instruction *IR_create_IR_Instruction(struct IR_Block *parent_block, enum IR_Instruction_type type) {
    struct IR_Instruction *instruction = calloc(1, sizeof(struct IR_Instruction));
    instruction->parent_block = parent_block;
    instruction->next = NULL;
    instruction->prev = NULL;

    instruction->type = type;
    instruction->id = -1;

    return instruction;
}

void IR_delete_IR_Instruction(struct IR_Instruction **instruction) {
    if(IR_INSTRUCTION_TYPE_JMP == (*instruction)->type && (*instruction)->operands.jmp.args) {
        vector_free(&(*instruction)->operands.jmp.args);
    }else if(IR_INSTRUCTION_TYPE_CALL == (*instruction)->type) {
        vector_free(&(*instruction)->operands.call.arguments);
    }
    free((*instruction));
    (*instruction) = NULL;
}

struct IR_Operand *IR_create_IR_Operand(enum IR_Operand_type type, struct IR_Instruction *definition_instruction, struct IR_Function *parent_function, int in_loop) {
    struct IR_Operand *operand = calloc(1, sizeof(struct IR_Operand));
    operand->type = type;
    operand->type_info = NULL;
    operand->definition_instruction = definition_instruction;
    operand->use_list = vector_create_vector(1, sizeof(struct IR_Instruction *));
    operand->in_loop = in_loop;

    if(parent_function) {
        vector_add(parent_function->operands, &operand);
    }

    if(IR_OPERAND_TYPE_VREG == type) {
        operand->data.vreg.variable = NULL;
        operand->data.vreg.reg = NULL;
    }else if(IR_OPERAND_TYPE_STACK_SLOT == type){
        operand->data.slot.stack_slot = NULL;
    }

    return operand;
}
void IR_delete_IR_Operand(struct IR_Operand **operand) {
    vector_free(&(*operand)->use_list);
    free((*operand));
    (*operand) = NULL;
}

void IR_init_live_interval(struct live_interval_t *interval, struct IR_Operand *operand, int start, int end, int weight) {
    interval->start = start;
    interval->end = end;
    interval->weight = weight;
    interval->use_score = 0;
    interval->vreg = operand;

    interval->is_spilled = false;
    interval->stack_slot = NULL;
    interval->assigned_register = NULL;
}


// add/remove

void IR_Module_add_function(struct IR_Module *module, struct IR_Function *function) {
    if(module->parent_project->main_function == NULL && str_view_eq_cstr(function->name, "main")) {
        module->parent_project->main_function = function;
        module->parent_project->main_module = module;
    }
    function->parent_module = module;
    vector_add(module->functions, &function);
}

void IR_Function_add_block(struct IR_Function *function, struct IR_Block *block) {
    if (function == NULL || block == NULL) return;

    block->next = NULL;
    if(NULL == function->head_block) {
        function->head_block = block;
        function->tail_block = block;
    }else {
        function->tail_block->next = block;
        block->prev = function->tail_block;
        function->tail_block = block;
    }
}

void IR_Function_add_parameter(struct IR_Function *function, struct IR_Operand *operand) {
}

void IR_Block_add_instruction(struct IR_Block *block, struct IR_Instruction *instruction) {
    if(NULL == block->head_instruction) {
        block->head_instruction = instruction;
        block->tail_instruction = instruction;
    }else {
        block->tail_instruction->next = instruction;
        instruction->prev = block->tail_instruction;
        block->tail_instruction = instruction;
    }
    ++block->instruction_count;
    ++block->parent_function->instruction_count;
}

void IR_Block_add_instruction_before(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction) {
    if(block->head_instruction == target_instruction) {
        target_instruction->prev = instruction;
        instruction->next = target_instruction;
        block->head_instruction = instruction;
    } else {
        target_instruction->prev->next = instruction;
        instruction->prev = target_instruction->prev;
        target_instruction->prev = instruction;
        instruction->next = target_instruction;
    }
}

void IR_Block_add_instruction_after(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction) {
    if(block->tail_instruction == target_instruction) {
        target_instruction->next = instruction;
        instruction->prev = target_instruction;
        block->tail_instruction = instruction;
    }else {
        target_instruction->next->prev = instruction;
        instruction->next = target_instruction->next;
        target_instruction->next = instruction;
        instruction->prev = target_instruction;
    }
}

// Helpers

struct IR_Operand *IR_create_new_vreg(struct IR_Function *parent_function, struct IR_Instruction *definition_instruction, struct symbol_t *variable, int in_loop) {
    struct IR_Operand *operand = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, definition_instruction, parent_function, in_loop);
    IR_init_live_interval(&operand->data.vreg.live_interval, operand, -1, -1, -1);

    operand->data.vreg.variable = variable;
    operand->data.vreg.vreg_id = parent_function->vreg_counter;
    operand->data.vreg.crosses_call = false;
    ++parent_function->vreg_counter;
    vector_add(parent_function->unique_vregs, &operand);
    return operand;
}

int IR_call_get_arg_index(struct IR_Instruction *call, struct IR_Operand *target_arg) {
    for(int i = 0; i < call->operands.call.arguments->element_count; ++i) {
        struct IR_Operand *curr = *(struct IR_Operand **) vector_get(call->operands.call.arguments, i);

        if(curr == target_arg) return i;
    }
    return -1;
}
