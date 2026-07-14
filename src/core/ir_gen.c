#include "core/ir_gen.h"
#include "h_vector.h"
#include <stdlib.h>

// create/free

struct IR_Module *IR_create_IR_Module() {
    struct IR_Module *module = calloc(1, sizeof(struct IR_Module));
    module->functions = vector_create_vector(1, sizeof(struct IR_Function *));

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

struct IR_Function *IR_create_IR_Function(char *name, char *mangled_name, int parameter_count) {
    struct IR_Function *function = calloc(1, sizeof(struct IR_Function));
    function->name = name;
    function->mangled_name = mangled_name;
    function->operands = vector_create_vector(4, sizeof(struct IR_Operand *));
    function->instruction_count = 0;
    function->unique_vregs = vector_create_vector(4, sizeof(struct IR_Operand *));
    function->parameter_count = parameter_count;
    function->stack_size = 0;

    return function;
}
void IR_delete_IR_Function(struct IR_Function **function) {
    for(struct IR_Block *block = (*function)->head_block;NULL != block; block = block->next) {
        IR_delete_IR_Block(&block);
    }

    for(int i = 0; i < (*function)->operands->element_count; ++i) {
        struct IR_Operand *operand = *(struct IR_Operand **)vector_get((*function)->operands, i);
        IR_delete_IR_Operand(&operand);
    }

    vector_free(&(*function)->unique_vregs);
    vector_free(&(*function)->operands);

    free((*function)->name);
    free((*function)->mangled_name);

    free((*function));
    (*function) = NULL;
}

struct IR_Block *IR_create_IR_Block(struct IR_Function *parent_function, char *name, char *mangled_name) {
    struct IR_Block *block = calloc(1, sizeof(struct IR_Block));
    block->name = name;
    block->mangled_name = mangled_name;
    block->parent_function = parent_function;
    block->predecessor = vector_create_vector(1, sizeof(struct IR_Block *));
    block->successors = vector_create_vector(1, sizeof(struct IR_Block *));

    block->block_in = NULL;
    block->block_out = NULL;

    block->head_instruction = NULL;
    block->tail_instruction = NULL;
    block->next = NULL;
    block->prev = NULL;

    block->instruction_count = 0;

    return block;
}
void IR_delete_IR_Block(struct IR_Block **block) {
    for(struct IR_Instruction *instruction = (*block)->head_instruction;NULL != instruction; instruction = instruction->next) {
        IR_delete_IR_Instruction(&instruction);
    }

    vector_free(&(*block)->predecessor);
    vector_free(&(*block)->successors);

    bitset_free(&(*block)->block_in);
    bitset_free(&(*block)->block_out);

    free((*block)->name);
    free((*block)->mangled_name);

    free((*block));
    (*block) = NULL;
}

struct IR_Instruction *IR_create_IR_Instruction(struct IR_Block *parent_block, enum IR_Instruction_type type, int id) {
    struct IR_Instruction *instruction = calloc(1, sizeof(struct IR_Instruction));
    instruction->parent_block = parent_block;
    instruction->next = NULL;
    instruction->prev = NULL;

    instruction->type = type;
    instruction->id = -1;

    return instruction;
}

void IR_delete_IR_Instruction(struct IR_Instruction **instruction) {
    free((*instruction));
    (*instruction) = NULL;
}

struct IR_Operand *IR_create_IR_Operand(enum IR_Operand_type type, struct IR_Instruction *definition_instruction) {
    struct IR_Operand *operand = calloc(1, sizeof(struct IR_Operand));
    operand->type = type;
    operand->definition_instruction = definition_instruction;
    operand->use_list = vector_create_vector(1, sizeof(struct IR_Instruction *));
    operand->is_address_taken = false;
    operand->is_hot = false;

    return operand;
}
void IR_delete_IR_Operand(struct IR_Operand **operand) {
    if(IR_OPERAND_TYPE_IMM == (*operand)->type) free((*operand)->data.imm_value);

    vector_free(&(*operand)->use_list);
    free((*operand));
    (*operand) = NULL;
}



// add/remove

void IR_Module_add_function(struct IR_Module *module, struct IR_Function *function) {
    vector_add(module->functions, &function);
}

void IR_Function_add_block(struct IR_Function *function, struct IR_Block *block) {
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

struct IR_Operand *IR_create_new_vreg(struct IR_Block *block, struct IR_Instruction *definition_instruction) {
    struct IR_Operand *operand = IR_create_IR_Operand(IR_OPERAND_TYPE_VREG, definition_instruction);
    int is_unique = 1;
    for(int i = 0;i < block->parent_function->unique_vregs->element_count; ++i) {
        struct IR_Operand *current = *(struct IR_Operand **)vector_get(block->parent_function->operands, i);
        if(current == operand || current->data.vreg_id == operand->data.vreg_id) {is_unique = 0; break;}
    }
    if(1 == is_unique) {
        vector_add(block->parent_function->unique_vregs, operand);
    }
    return operand;
}
