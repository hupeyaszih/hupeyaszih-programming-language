#include "ir_gen.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>

/// Create & Delete Functions
struct IR_Module *IR_create_IR_Module() {
    struct IR_Module *module = calloc(1, sizeof(struct IR_Module));
    if(NULL == module) return NULL;

    module->entry_function = NULL;
    module->functions = vector_create_vector(1, sizeof(struct IR_Function *));

    return module;
}
int IR_delete_IR_Module(struct IR_Module **module) {
    if(NULL == module || NULL == *module) return 1;
    for(int i = 0;i < (*module)->functions->element_count; ++i) {
        struct IR_Function **fn = vector_get((*module)->functions, i);
        IR_delete_IR_Function(fn);
    }
    vector_free(&(*module)->functions);

    free((*module));
    *module = NULL;

    return 0;
}

struct IR_Function *IR_create_IR_Function() {
    struct IR_Function *function = calloc(1, sizeof(struct IR_Function));
    if(NULL == function) return NULL;
    function->operands = vector_create_vector(1, sizeof(struct IR_Operand *));
    function->entry_block = NULL;
    function->exit_block = NULL;
    function->tail_block = NULL;
    function->head_block = NULL;
    function->vg_counter = 0;

    return function;
}
int IR_delete_IR_Function(struct IR_Function **function){
    if(NULL == function || NULL == *function) return 1;

    struct IR_Block *curr = (*function)->head_block;
    while(curr) {
        struct IR_Block *next = curr->next_in_func;
        IR_delete_IR_Block(&curr);
        curr = next;
    }

    for(int i = 0;i < (*function)->operands->element_count; ++i) {
        struct IR_Operand **operand = vector_get((*function)->operands, i);
        IR_delete_IR_Operand(operand);
    }
    vector_free(&(*function)->operands);

    free((*function)->name);
    free((*function)->label);
    free(*function);
    *function = NULL;
    return 0;
}

struct IR_Block *IR_create_IR_Block() {
    struct IR_Block *block = calloc(1, sizeof(struct IR_Block));
    if(NULL == block) return NULL;
    block->dom_frontier = vector_create_vector(1, sizeof(struct IR_Block *));
    if(NULL == block->dom_frontier) goto ir_create_block_cleanup;
    block->predecessor = vector_create_vector(1, sizeof(struct IR_Block *));
    if(NULL == block->predecessor) goto ir_create_block_cleanup;
    block->successors = vector_create_vector(1, sizeof(struct IR_Block *));
    if(NULL == block->successors) goto ir_create_block_cleanup;


    return block;

ir_create_block_cleanup:
    IR_delete_IR_Block(&block);
    return NULL;
}

int IR_delete_IR_Block(struct IR_Block **block) {
    if(NULL == block || NULL == *block) return 1;
    vector_free(&(*block)->dom_frontier);
    vector_free(&(*block)->predecessor);
    vector_free(&(*block)->successors);

    struct IR_Instruction *curr = (*block)->entry_instruction;
    while(curr) {
        struct IR_Instruction *next = curr->next;
        IR_delete_IR_Instruction(&curr);
        curr = next;
    }

    free((*block)->label);

    free(*block);
    *block = NULL;
    return 0;
}

struct IR_Instruction *IR_create_IR_Instruction(enum IR_InstructionType instruction_type) {
    struct IR_Instruction *instruction = calloc(1, sizeof(struct IR_Instruction));
    if(NULL == instruction) return NULL;
    instruction->type = instruction_type;

    if(IR_INSTRUCTION_TYPE_PHI == instruction->type) {
        instruction->data.phi_inputs = vector_create_vector(1, sizeof(struct IR_PhiInput *));
    }else if(IR_INSTRUCTION_TYPE_JMP == instruction->type) {
        instruction->data.target_block = NULL;
    }else if(IR_INSTRUCTION_TYPE_BR_COND == instruction->type) {
        instruction->data.br_target.false_block = NULL;
        instruction->data.br_target.true_block = NULL;
    }else if(IR_INSTRUCTION_TYPE_CALL == instruction->type) {
        instruction->data.call_info.arguments = vector_create_vector(1, sizeof(struct IR_Operand *));
    }

    return instruction;
}
int IR_delete_IR_Instruction(struct IR_Instruction **instruction) {
    if(NULL == instruction || NULL == *instruction) return 1;

    if(IR_INSTRUCTION_TYPE_PHI == (*instruction)->type) {
        for(int i = 0; i < (*instruction)->data.phi_inputs->element_count; ++i) {
            struct IR_PhiInput **phi_in = vector_get((*instruction)->data.phi_inputs, i);
            IR_delete_IR_PhiInput(phi_in);
        }
        vector_free(&(*instruction)->data.phi_inputs);
    }else if (IR_INSTRUCTION_TYPE_CALL == (*instruction)->type) {
        vector_free(&(*instruction)->data.call_info.arguments);
        free((*instruction)->data.call_info.function_label);
    }else if(IR_INSTRUCTION_TYPE_ASM) {
        free((*instruction)->data.asm_code);
    }

    free((*instruction));
    *instruction = NULL;
    return 0;
}

struct IR_PhiInput *IR_create_IR_PhiInput(struct IR_Block *from_block, struct IR_Operand *operand){
    struct IR_PhiInput *phi_input = calloc(1, sizeof(struct IR_PhiInput));
    if(NULL == phi_input) return NULL;
    phi_input->from_block = from_block;
    phi_input->operand = operand;

    return phi_input;
}
int IR_delete_IR_PhiInput(struct IR_PhiInput **phi_input) {
    if(NULL == phi_input || NULL == *phi_input) return 1;
    // if((*phi_input)->operand) IR_delete_IR_Operand(&(*phi_input)->operand);

    free((*phi_input));
    *phi_input = NULL;
    return 0;
}

struct IR_Operand *IR_create_IR_Operand(enum IR_OperandType operand_type, struct IR_Function *parent_function) {
    struct IR_Operand *operand = calloc(1, sizeof(struct IR_Operand));
    if(NULL == operand) return NULL;
    operand->type = operand_type;
    vector_add(parent_function->operands, &operand);
    return operand;
}
int IR_delete_IR_Operand(struct IR_Operand **operand) {
    if(NULL == operand || NULL == *operand) return 1;
    switch ((*operand)->type) {
        case IR_OPERAND_TYPE_LABEL:
            vector_free(&(*operand)->value.targets);
            break;
        case IR_OPERAND_TYPE_IMM:
            free((*operand)->value.imm);
            break;
        case IR_OPERAND_TYPE_VREG: 
            break;
        case IR_OPERAND_TYPE_UNDEFINED:
            break;
    }

    free((*operand));
    (*operand) = NULL;
    return 0;
}

/// Other Functions
int IR_Function_add_block(struct IR_Function *restrict function, struct IR_Block *restrict block) {
    if (!function || !block) return -1;
    if (NULL == function->head_block) {
        function->entry_block = block;
        function->head_block = block;
        function->tail_block = block;
        block->next_in_func = NULL;
    } else {
        function->tail_block->next_in_func = block;
        function->tail_block = block;
        block->next_in_func = NULL;
    }

    return 0;
}

int IR_Module_add_function(struct IR_Module *restrict module, struct IR_Function *restrict function){
    if(NULL == module || NULL == function) return -1;

    if(NULL == module->entry_function) {
        module->entry_function = function;
    }
    vector_add(module->functions, (void*)&function);

    return 0;
}

int IR_Block_add_instruction(struct IR_Block *restrict block, struct IR_Instruction *restrict instruction) {
    if(NULL == block || NULL == instruction) return 1;

    if(NULL == block->entry_instruction) {
        block->entry_instruction = instruction;
        block->tail_instruction = instruction; 
    } else {
        block->tail_instruction->next = instruction;
        instruction->prev = block->tail_instruction; 
        block->tail_instruction = instruction;
    }
    return 0;
}
