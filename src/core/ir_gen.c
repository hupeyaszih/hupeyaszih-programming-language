#include "core/ir_gen.h"
#include "core/symbol_table.h"
#include "h_arena.h"
#include "h_bitset.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// create/free

struct stack_slot_t *IR_create_stack_slot(struct arena *arena, struct type_info *type, struct IR_Function *function, bool is_argument) {
    struct stack_slot_t *slot = arena_alloc(arena, sizeof(struct stack_slot_t));
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

struct IR_Project *IR_create_IR_Project(struct arena *arena, struct arena *temp_arena) {
    struct IR_Project *project = arena_alloc(arena, sizeof(struct IR_Project));
    project->modules = vector_create_vector(arena, 1, sizeof(struct IR_Module *));
    project->arena = arena;
    project->temp_arena = temp_arena;

    return project;
}

struct IR_Module *IR_create_IR_Module(struct arena *arena, char *name) {
    struct IR_Module *module = arena_alloc(arena, sizeof(struct IR_Module));
    module->functions = vector_create_vector(arena, 4, sizeof(struct IR_Function *));
    module->globals = vector_create_vector(arena, 4, sizeof(struct IR_Operand *));
    module->name = name;

    return module;
}

struct IR_Function *IR_create_IR_Function(struct arena *arena, struct bitset_t *flags, struct str_view name, struct str_view mangled_name, int parameter_count) {
    struct IR_Function *function = arena_alloc(arena, sizeof(struct IR_Function));
    function->name = name;
    function->mangled_name = mangled_name;


    function->flags = bitset_create(arena, flags->max_element_count);
    bitset_copy(function->flags, flags);

    function->parameters   = vector_create_vector(arena, 6, sizeof(struct IR_Operand *));
    function->operands     = vector_create_vector(arena, 16, sizeof(struct IR_Operand *));
    function->stack_slots  = vector_create_vector(arena, 8, sizeof(struct stack_slot_t *));
    function->unique_vregs = vector_create_vector(arena, 16, sizeof(struct IR_Operand *));
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

struct IR_Block *IR_create_IR_Block(struct arena *arena, struct IR_Function *parent_function, struct str_view mangled_name) {
    struct IR_Block *block = arena_alloc(arena, sizeof(struct IR_Block));
    block->mangled_name = mangled_name;
    block->parent_function = parent_function;
    block->predecessor = vector_create_vector(arena, 1, sizeof(struct IR_Block *));
    block->successors  = vector_create_vector(arena, 1, sizeof(struct IR_Block *));

    block->uses = NULL;
    block->defs = NULL;
    block->live_in = NULL;
    block->live_out = NULL;

    block->head_instruction = NULL;
    block->tail_instruction = NULL;
    block->loop_tail_instruction = NULL;
    block->loop_head_instruction = NULL;
    block->next = NULL;
    block->prev = NULL;

    block->instruction_count = 0;
    block->in_loop = 0;

    block->params = vector_create_vector(arena, 2, sizeof(struct IR_Operand *));

    return block;
}

struct IR_Instruction *IR_create_IR_Instruction(struct arena *arena, struct IR_Block *parent_block, enum IR_Instruction_type type) {
    struct IR_Instruction *instruction = arena_alloc(arena, sizeof(struct IR_Instruction));
    instruction->parent_block = parent_block;
    instruction->next = NULL;
    instruction->prev = NULL;

    instruction->type = type;
    instruction->id = -1;

    return instruction;
}


struct IR_Operand *IR_create_IR_Operand(struct arena *arena, enum IR_Operand_type type, struct IR_Instruction *definition_instruction, struct IR_Function *parent_function, int in_loop) {
    struct IR_Operand *operand = arena_alloc(arena, sizeof(struct IR_Operand));
    operand->type = type;
    operand->type_info = NULL;
    operand->definition_instruction = definition_instruction;
    operand->use_list = vector_create_vector(arena, 1, sizeof(struct IR_Instruction *));
    operand->in_loop = in_loop;
    operand->constant = false;

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

void IR_Module_add_global(struct IR_Module *module, struct IR_Operand *global) {
    vector_add(module->globals, &global);
}

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

void IR_Block_add_instruction(struct IR_Block *block, struct IR_Instruction *instruction) {
    if(!block) return;
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

void IR_Block_remove_instruction(struct IR_Block *block, struct IR_Instruction *instruction) {
    if (NULL == block || NULL == instruction) return;

    instruction->type = IR_INSTRUCTION_TYPE_NOP;
}
// Helpers

struct IR_Operand *IR_create_new_vreg(struct arena *arena, struct IR_Function *parent_function, struct IR_Instruction *definition_instruction, struct symbol_t *variable, int in_loop) {
    struct IR_Operand *operand = IR_create_IR_Operand(arena, IR_OPERAND_TYPE_VREG, definition_instruction, parent_function, in_loop);
    IR_init_live_interval(&operand->data.vreg.live_interval, operand, -1, -1, -1);

    operand->data.vreg.variable = variable;
    operand->data.vreg.vreg_id = parent_function->vreg_counter;
    operand->data.vreg.crosses_call = false;
    ++parent_function->vreg_counter;
    vector_add(parent_function->unique_vregs, &operand);
    return operand;
}

struct IR_Operand *IR_create_new_global(struct arena *arena, struct IR_Module *module, struct str_view value, struct symbol_t *variable, bool is_bss, struct type_table *type_table) {
    struct IR_Operand *global = IR_create_IR_Operand(arena, IR_OPERAND_TYPE_GLOBAL, NULL, NULL, 0);
    global->data.global.value = value;
    global->data.global.is_bss = is_bss;

    global->data.global.name = IR_Module_create_global_name(module, arena);

    if(variable){
        global->data.global.variable = variable;
        global->type_info = variable->type;
        variable->global = global;
    }else {
        global->data.global.variable = NULL;
        global->type_info = NULL;
    }


    global->data.global.kind = IR_get_global_kind(type_table, global);
    return global;
}

int IR_call_get_arg_index(struct IR_Instruction *call, struct IR_Operand *target_arg) {
    for(int i = 0; i < call->operands.call.arguments->element_count; ++i) {
        struct IR_Operand *curr = *(struct IR_Operand **) vector_get(call->operands.call.arguments, i);

        if(curr == target_arg) return i;
    }
    return -1;
}

struct str_view IR_Module_create_global_name(struct IR_Module *module, struct arena *arena) {
    char id[32];
    snprintf(id, sizeof(id), "%ld", module->globals->element_count+1);
    
    size_t size = strlen(id) + 5;
    
    char *mangled_name = arena_alloc(arena, sizeof(char) * size);
    if (!mangled_name) {
        return (struct str_view){ NULL, 0 };
    }

    snprintf(mangled_name, size, "glb_%s", id);

    return str_view_make(mangled_name, size);
}

enum IR_Global_Kind IR_get_global_kind(struct type_table *table, struct IR_Operand *global){
    if(type_table_is_info_string(table, global->type_info)) return IR_GLOBAL_KIND_STRING;
    if(global->constant) return IR_GLOBAL_KIND_CONSTANT;
    return IR_GLOBAL_KIND_VARIABLE;
}
