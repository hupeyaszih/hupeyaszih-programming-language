#ifndef H_IR_GEN_H
#define H_IR_GEN_H

#include "backend/codegen.h"
#include "core/symbol_table.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>

enum IR_Instruction_type {
    IR_INSTRUCTION_TYPE_NOP,
    IR_INSTRUCTION_TYPE_CAST,
    IR_INSTRUCTION_TYPE_MOV,
    IR_INSTRUCTION_TYPE_ALLOCA,
    IR_INSTRUCTION_TYPE_LOAD,
    IR_INSTRUCTION_TYPE_STORE,
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
    IR_INSTRUCTION_TYPE_PLUS,
    IR_INSTRUCTION_TYPE_MINUS,
    IR_INSTRUCTION_TYPE_DIVIDE,
    IR_INSTRUCTION_TYPE_MUL,
    IR_INSTRUCTION_TYPE_ASM,
    IR_INSTRUCTION_TYPE_CALL,
    IR_INSTRUCTION_TYPE_BR,
    IR_INSTRUCTION_TYPE_JMP,
    IR_INSTRUCTION_TYPE_RET, // return
    IR_INSTRUCTION_TYPE_UNDEFINED
};

enum IR_Instructions_Operands_type {
    IR_INSTRUCTIONS_OPERANDS_TYPE_JMP,
    IR_INSTRUCTIONS_OPERANDS_TYPE_RET,
    IR_INSTRUCTIONS_OPERANDS_TYPE_ALLOCA,
    IR_INSTRUCTIONS_OPERANDS_TYPE_TRIPLE,
    IR_INSTRUCTIONS_OPERANDS_TYPE_DOUBLE,
    IR_INSTRUCTIONS_OPERANDS_TYPE_CALL,
    IR_INSTRUCTIONS_OPERANDS_TYPE_BR,
    IR_INSTRUCTIONS_OPERANDS_TYPE_ASM,
    IR_INSTRUCTIONS_OPERANDS_TYPE_UNDEFINED
};

static inline enum IR_Instructions_Operands_type IR_get_Instructions_Operands_type(enum IR_Instruction_type type) {
    switch (type) {
        case IR_INSTRUCTION_TYPE_UNARY_BANG:
        case IR_INSTRUCTION_TYPE_UNARY_MINUS:
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE:
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:
        case IR_INSTRUCTION_TYPE_CAST:
        case IR_INSTRUCTION_TYPE_MOV:
        case IR_INSTRUCTION_TYPE_LOAD:
        case IR_INSTRUCTION_TYPE_STORE: return IR_INSTRUCTIONS_OPERANDS_TYPE_DOUBLE;

        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:
        case IR_INSTRUCTION_TYPE_LESS:
        case IR_INSTRUCTION_TYPE_GREATER:
        case IR_INSTRUCTION_TYPE_PLUS:
        case IR_INSTRUCTION_TYPE_MINUS:
        case IR_INSTRUCTION_TYPE_MUL:
        case IR_INSTRUCTION_TYPE_DIVIDE: return IR_INSTRUCTIONS_OPERANDS_TYPE_TRIPLE;

        case IR_INSTRUCTION_TYPE_ALLOCA: return IR_INSTRUCTIONS_OPERANDS_TYPE_ALLOCA;
        case IR_INSTRUCTION_TYPE_ASM: return IR_INSTRUCTIONS_OPERANDS_TYPE_ASM;
        case IR_INSTRUCTION_TYPE_CALL: return IR_INSTRUCTIONS_OPERANDS_TYPE_CALL;
        case IR_INSTRUCTION_TYPE_BR: return IR_INSTRUCTIONS_OPERANDS_TYPE_BR;
        case IR_INSTRUCTION_TYPE_JMP: return IR_INSTRUCTIONS_OPERANDS_TYPE_JMP;
        case IR_INSTRUCTION_TYPE_RET: return IR_INSTRUCTIONS_OPERANDS_TYPE_RET;

        case IR_INSTRUCTION_TYPE_UNDEFINED:
        case IR_INSTRUCTION_TYPE_NOP: return IR_INSTRUCTIONS_OPERANDS_TYPE_UNDEFINED;
    }

    return IR_INSTRUCTIONS_OPERANDS_TYPE_UNDEFINED;
}

enum IR_Operand_type {
    IR_OPERAND_TYPE_UNDEFINED,
    IR_OPERAND_TYPE_IMM,
    IR_OPERAND_TYPE_VREG,
    IR_OPERAND_TYPE_STACK_SLOT,
    IR_OPERAND_TYPE_LABEL
};

struct stack_slot_t {
    int stack_offset;
    struct type_info *type;
    struct IR_Operand *current_vreg;
    bool is_busy;
    bool is_argument;
};

struct live_interval_t {
    struct IR_Operand *vreg;
    struct register_t *assigned_register;
    struct stack_slot_t *stack_slot;

    int start, end;
    int use_score, weight;

    bool is_spilled;
};

struct IR_Operand {
    union{
        struct str_view imm_value;
        struct str_view mangled_label_name;
        struct {
            struct symbol_t *variable;
            struct register_t *reg;
            int vreg_id;
            struct live_interval_t live_interval;
            bool crosses_call;
        } vreg;
        struct {
            struct live_interval_t live_interval;
            struct stack_slot_t *stack_slot;
        } slot;
    } data;

    struct IR_Instruction *definition_instruction;
    struct vector_t *use_list; // struct IR_Instruction *    (All instructions which use the operand)
    struct type_info *type_info;

    enum IR_Operand_type type;
    int in_loop;
    bool constant;
};


struct IR_Instruction {

    union {

        struct {
            struct IR_Block *target_block; // For jump
            struct vector_t *args; // struct IR_Operand *
        } jmp;

        struct {
            struct IR_Function *function;
            struct IR_Operand *return_value;
        } ret;

        struct {
            struct IR_Operand *source_1;
            struct IR_Operand *source_2;
            struct IR_Operand *destination;
        }triple_operands;

        struct {
            struct type_info *type_info;
            struct IR_Operand *destination;
        }alloca;

        struct {
            struct IR_Operand *source_1;
            struct IR_Operand *destination;
        }double_operands;

        struct {
            struct vector_t *arguments; // struct IR_Operand *    (Argument list)
            struct IR_Function *target_function;
            struct IR_Operand *return_val;
            struct bitset_t *across_registers;
        }call;

        struct {
            struct IR_Operand *condition;
            struct IR_Block *true_block;
            struct vector_t *true_args; // struct IR_Operand *

            struct IR_Block *false_block;
            struct vector_t *false_args; // struct IR_Operand *
        }br;

        struct {
            struct str_view asm_imm;
        } asm_operands;

    }operands;
    struct IR_Block *parent_block;

    struct IR_Instruction *next;
    struct IR_Instruction *prev;

    enum IR_Instruction_type type;

    int id;

    bool has_side_effect; // If it is true, the instruction has a side effect (call etc.)
    bool is_dead; // If it is true, the instruction will be destroyed in dead code elimination    (If the flag is true, the instruction will be destroyed)
};


struct IR_Block {
    struct IR_Function *parent_function;

    struct IR_Instruction *head_instruction;
    struct IR_Instruction *tail_instruction;

    struct IR_Block *next;
    struct IR_Block *prev;

    struct vector_t *predecessor;  // struct IR_Block *
    struct vector_t *successors;   // struct IR_Block *

    struct bitset_t *use;       // All vreg_ids used in the block
    struct bitset_t *def;       // All vreg_ids defined in the block

    struct vector_t *params; // struct IR_Operand *

    struct str_view mangled_name;
    int in_loop;
    struct IR_Instruction *loop_tail_instruction;
    struct IR_Instruction *loop_head_instruction;
    int instruction_count;
};


struct IR_Function {
    struct IR_Module *parent_module;
    struct IR_Block *head_block;
    struct IR_Block *tail_block;

    struct vector_t *parameters; // struct IR_Operand *
    struct vector_t *operands; /* struct IR_Function *                                  
                               (NOTE: All operands belong to a function. Only a function can free an operand!)*/
                                  
    struct vector_t *stack_slots; //  struct stack_slot_t *  (List of all stack_slots in the function. Only a function can free an stack_slot)
                                  
    struct vector_t *unique_vregs; // struct IR_Operand *    (List of all unique vregs in the function)

    struct bitset_t *used_callee_saved_registers;
    struct bitset_t *used_caller_saved_registers;
    struct bitset_t *directly_used_caller_saved_registers;

    struct IR_Operand *return_value;
    struct bitset_t *flags;

    struct str_view name, mangled_name;

    int instruction_count; // Total instruction count in the function
    int parameter_count;
    int vreg_counter;
    int stack_size;
    int stack_size_for_args;

    bool is_visiting;          // using in "register_allocator_compute_caller_saved_registers"
    bool is_fully_processed;   // using in "register_allocator_compute_caller_saved_registers"

};


struct IR_Module {
    char *name;
    struct vector_t *functions; // struct IR_Function *
    struct IR_Project *parent_project;
};

struct IR_Project {
    struct arena *arena;
    struct arena *temp_arena;

    struct vector_t *modules; // struct IR_Module *
    struct IR_Function *main_function;
    struct IR_Module *main_module;
};


// create/free
struct stack_slot_t *IR_create_stack_slot(struct arena *arena, struct type_info *type, struct IR_Function *function, bool is_argument);
struct IR_Project *IR_create_IR_Project(struct arena *arena, struct arena *temp_arena);
struct IR_Module *IR_create_IR_Module(struct arena *arena, char *name);
struct IR_Function *IR_create_IR_Function(struct arena *arena, struct bitset_t *flags, struct str_view name, struct str_view mangled_name, int parameter_count);
struct IR_Block *IR_create_IR_Block(struct arena *arena, struct IR_Function *parent_function, struct str_view mangled_name);
struct IR_Instruction *IR_create_IR_Instruction(struct arena *arena, struct IR_Block *parent_block, enum IR_Instruction_type type);
struct IR_Operand *IR_create_IR_Operand(struct arena *arena, enum IR_Operand_type type, struct IR_Instruction *definition_instruction, struct IR_Function *parent_function, int in_loop);

void IR_init_live_interval(struct live_interval_t *interval, struct IR_Operand *operand, int start, int end, int weight);

// add/remove
void IR_Module_add_function(struct IR_Module *module, struct IR_Function *function);
void IR_Function_add_block(struct IR_Function *function, struct IR_Block *block);
void IR_Block_add_instruction(struct IR_Block *block, struct IR_Instruction *instruction);
void IR_Block_add_instruction_before(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction);
void IR_Block_add_instruction_after(struct IR_Block *block, struct IR_Instruction *target_instruction, struct IR_Instruction *instruction);
void IR_Block_remove_instruction(struct IR_Block *block, struct IR_Instruction *instruction);

// calculation functions

/*   ...   */

// Helpers

struct IR_Operand *IR_create_new_vreg(struct arena *arena, struct IR_Function *parent_function, struct IR_Instruction *definition_instruction, struct symbol_t *variable, int in_loop);

int IR_call_get_arg_index(struct IR_Instruction *call, struct IR_Operand *target_arg);

// Other Functions
static inline int IR_get_instruction_cost(enum IR_Instruction_type type) {
    switch (type) {
        case IR_INSTRUCTION_TYPE_NOP:
            return 0;
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
        case IR_INSTRUCTION_TYPE_ALLOCA:
        case IR_INSTRUCTION_TYPE_STORE:
        case IR_INSTRUCTION_TYPE_LOAD:
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
