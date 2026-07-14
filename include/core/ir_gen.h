#ifndef H_IR_GEN_H
#define H_IR_GEN_H

#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>

enum IR_Instruction_type {
    IR_INSTRUCTION_TYPE_MOV, // STORE/LOAD/COPY etc.
    IR_INSTRUCTION_TYPE_EQUAL_EQUAL = 1,
    IR_INSTRUCTION_TYPE_BANG_EQUAL = 2,
    IR_INSTRUCTION_TYPE_LESS_EQUAL = 3,
    IR_INSTRUCTION_TYPE_GREATER_EQUAL = 4,
    IR_INSTRUCTION_TYPE_LESS = 5,
    IR_INSTRUCTION_TYPE_GREATER = 6,
    IR_INSTRUCTION_TYPE_UNARY_BANG,
    IR_INSTRUCTION_TYPE_UNARY_MINUS,
    IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF,
    IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE,
    IR_INSTRUCTION_TYPE_PLUS,
    IR_INSTRUCTION_TYPE_MINUS,
    IR_INSTRUCTION_TYPE_DIVIDE,
    IR_INSTRUCTION_TYPE_MUL,
    IR_INSTRUCTION_TYPE_ASM,
    IR_INSTRUCTION_TYPE_CALL,
    IR_INSTRUCTION_TYPE_BR,
    IR_INSTRUCTION_TYPE_JMP,
    IR_INSTRUCTION_TYPE_UNDEFINED
};



enum IR_Operand_type {
    IR_OPERAND_TYPE_IMM,
    IR_OPERAND_TYPE_VREG,
    IR_OPERAND_TYPE_STACK
};


struct IR_Operand {
    enum IR_Operand_type type;
    union{
        char *imm_value;
        int vreg_id;
        int stack_offset;
    } data;

    struct IR_Instruction *definition_instruction;
    struct vector_t *use_list; // struct IR_Instruction *    (All instructions which use the operand)

    bool is_address_taken; // If it is true, this operand must be in the memory
    bool is_hot; // If it is true, this operand must be in a register
};


struct IR_Instruction {
    struct IR_Block *parent_block;

    union {

        struct IR_Block *target_block; // For jump

        struct {
            struct IR_Operand *source_1;
            struct IR_Operand *source_2;
            struct IR_Operand *destination;
        }triple_operands;


        struct {
            struct vector_t *arguments; // struct IR_Operand *    (Argument list)
            struct IR_Function *target_function;
        }call;

        struct {
            struct IR_Operand *condition;
            struct IR_Block *true_block;
            struct IR_Block *false_block;
        }br;

    }operands;

    struct IR_Instruction *next;
    struct IR_Instruction *prev;

    enum IR_Instruction_type type;

    int id;

    bool has_side_effect; // If it is true, the instruction has a side effect (call etc.)
    bool is_dead; // If it is true, the instruction will be destroyed because of dead code elimination    (If the flag is true, the instruction will be destroyed)
};


struct IR_Block {
    struct IR_Function *parent_function;

    struct IR_Instruction *head_instruction;
    struct IR_Instruction *tail_instruction;

    struct IR_Block *next;
    struct IR_Block *prev;

    struct vector_t *predecessor;  // struct IR_Block *
    struct vector_t *successors;   // struct IR_Block *

    struct bitset_t *block_in;  // All vreg_ids the block use which come from another block
    struct bitset_t *block_out; // All vreg_ids that the block gives out alive

    int instruction_count;
    char *name, *mangled_name;
};


struct IR_Function {
    struct IR_Block *head_block;
    struct IR_Block *tail_block;

    struct vector_t *operands; /* struct IR_Function *                                  
                               (NOTE: All operands belong to a function. Only a function can free an operand!)*/

    struct vector_t *unique_vregs; // struct IR_Operand *    (List of all unique vregs in the function)

    int instruction_count; // Total instruction count in the function
    int parameter_count;
    int vreg_counter;
    int stack_size;
    char *name, *mangled_name;
};


struct IR_Module {
    struct vector_t *functions; // struct IR_Function *
};


// create/free
struct IR_Module *IR_create_IR_Module();
void IR_delete_IR_Module(struct IR_Module **module);

struct IR_Function *IR_create_IR_Function(char *name, char *mangled_name, int parameter_count);
void IR_delete_IR_Function(struct IR_Function **function);

struct IR_Block *IR_create_IR_Block(struct IR_Function *parent_function, char *name, char *mangled_name);
void IR_delete_IR_Block(struct IR_Block **block);

struct IR_Instruction *IR_create_IR_Instruction(struct IR_Block *parent_block, enum IR_Instruction_type type, int id);
void IR_delete_IR_Instruction(struct IR_Instruction **instruction);

struct IR_Operand *IR_create_IR_Operand(enum IR_Operand_type type, struct IR_Instruction *definition_instruction);
void IR_delete_IR_Operand(struct IR_Operand **operand);

// add/remove
void IR_Module_add_function(struct IR_Module *module, struct IR_Function *function);
void IR_Function_add_block(struct IR_Function *function, struct IR_Block *block);
void IR_Block_add_instruction(struct IR_Block *block, struct IR_Instruction *instruction);
void IR_Block_add_instruction_before(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction);
void IR_Block_add_instruction_after(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction);

// calculation functions

/*   ...   */

// Helpers

struct IR_Operand *IR_create_new_vreg(struct IR_Block *block, struct IR_Instruction *definition_instruction);

// Other Functions
static inline int IR_get_instruction_cost(enum IR_Instruction_type type) {
    switch (type) {
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS:
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
            return 1;
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE:
            return 2;
        case IR_INSTRUCTION_TYPE_MUL:
            return 3;
        case IR_INSTRUCTION_TYPE_DIVIDE:
            return 10;
        case IR_INSTRUCTION_TYPE_BR:
        case IR_INSTRUCTION_TYPE_JMP:
            return 6;
        case IR_INSTRUCTION_TYPE_CALL:
            return 20;
        case IR_INSTRUCTION_TYPE_ASM:
            return 10;
        case IR_INSTRUCTION_TYPE_UNDEFINED:
            return 1;

    }
    return 1;
}
#endif 
