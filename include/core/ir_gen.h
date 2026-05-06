#ifndef IR_H
#define IR_H

#include "h_vector.h"
#include <stddef.h>

enum IR_OperandType{
    IR_OPERAND_TYPE_VREG,
    IR_OPERAND_TYPE_IMM,
    IR_OPERAND_TYPE_LABEL,
    IR_OPERAND_TYPE_UNDEFINED
};

enum IR_InstructionType {
    IR_INSTRUCTION_TYPE_EQUAL_EQUAL,
    IR_INSTRUCTION_TYPE_BANG_EQUAL,
    IR_INSTRUCTION_TYPE_LESS_EQUAL,
    IR_INSTRUCTION_TYPE_GREATER_EQUAL,
    IR_INSTRUCTION_TYPE_LESS,
    IR_INSTRUCTION_TYPE_GREATER,
    IR_INSTRUCTION_TYPE_UNARY_BANG,
    IR_INSTRUCTION_TYPE_UNARY_MINUS,
    IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF,
    IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE,
    IR_INSTRUCTION_TYPE_ADD,
    IR_INSTRUCTION_TYPE_SUB,
    IR_INSTRUCTION_TYPE_MUL,
    IR_INSTRUCTION_TYPE_DIV,
    IR_INSTRUCTION_TYPE_LOAD,
    IR_INSTRUCTION_TYPE_STORE,
    IR_INSTRUCTION_TYPE_JMP,
    IR_INSTRUCTION_TYPE_PHI,
    IR_INSTRUCTION_TYPE_RET,
    IR_INSTRUCTION_TYPE_CALL,
    IR_INSTRUCTION_TYPE_ALLOCA,
    IR_INSTRUCTION_TYPE_COPY,
    IR_INSTRUCTION_TYPE_BR_COND,
    IR_INSTRUCTION_TYPE_ASM
};

struct IR_Operand {
    struct type_info *type_info;
    enum IR_OperandType type;
    union{
        int vreg_id;
        char *imm;
        struct vector_t *targets; // IR_Block
    } value;
};

struct IR_PhiInput {
    struct IR_Block *from_block;
    struct IR_Operand *operand;
};

struct IR_Instruction {
    union {
        struct vector_t *phi_inputs; // IR_PhiInput *
        struct IR_Block *target_block; // For jump
        struct {
            struct IR_Block *true_block;
            struct IR_Block *false_block;
        }br_target;

        struct {
            char *function_label;
            struct vector_t *arguments; // IR_Operand * 
        } call_info;

        char *asm_code;
    }data;

    struct IR_Operand *destination;
    struct IR_Operand *source_1;
    struct IR_Operand *source_2;
    
    struct IR_Instruction *prev;
    struct IR_Instruction *next;

    struct IR_Block *parent_block;
    enum IR_InstructionType type;
    int id;
};

struct IR_Block {
    struct IR_Block *idom;
    struct IR_Block *next_in_func; // Don't use in graph !!!

    struct IR_Instruction *entry_instruction;
    struct IR_Instruction *tail_instruction;

    struct vector_t *predecessor;  // IR_Block
    struct vector_t *successors;   // IR_Block

    struct vector_t *dom_frontier; // IR_Block

    char *label;
    int dfn;
    int id;
};

struct IR_Function {
    struct IR_Block *entry_block, *exit_block;
    struct IR_Block *head_block;
    struct IR_Block *tail_block;

    struct vector_t *operands; // Don't use in graph !!!

    int vg_counter;
    int parameter_count;
    char *name;
    char *label;
};

struct IR_Module {
    struct vector_t *functions; // IR_Function
    struct IR_Function *entry_function;
};


/// Create & Delete Functions
struct IR_Module *IR_create_IR_Module();
int IR_delete_IR_Module(struct IR_Module **module);

struct IR_Function *IR_create_IR_Function();
int IR_delete_IR_Function(struct IR_Function **function);

struct IR_Block *IR_create_IR_Block();
int IR_delete_IR_Block(struct IR_Block **block);

struct IR_Instruction *IR_create_IR_Instruction(enum IR_InstructionType instruction_type);
int IR_delete_IR_Instruction(struct IR_Instruction **instruction);

struct IR_PhiInput *IR_create_IR_PhiInput(struct IR_Block *from_block, struct IR_Operand *operand);
int IR_delete_IR_PhiInput(struct IR_PhiInput **phi_input);

struct IR_Operand *IR_create_IR_Operand(enum IR_OperandType operand_type, struct IR_Function *parent_function);
int IR_delete_IR_Operand(struct IR_Operand **operand);

/// Other Functions
int IR_Block_add_instruction(struct IR_Block *restrict block, struct IR_Instruction *restrict instruction);
int IR_Function_add_block(struct IR_Function *restrict function, struct IR_Block *restrict block);
int IR_Module_add_function(struct IR_Module *restrict module, struct IR_Function *restrict function);

// Helpers
#endif
