#ifndef CODEGEN_H
#define CODEGEN_H

#include "backend/x86_64/x86_64_linux.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>

struct IR_Project;
struct IR_Module;
struct IR_Function;
struct IR_Instruction;

enum register_bank {
    REGISTER_BANK_GPR,
    REGISTER_BANK_VECTOR
};

enum register_size {
    REGISTER_SIZE_UNDEFINED = -1,
    REGISTER_SIZE_8,
    REGISTER_SIZE_16,
    REGISTER_SIZE_32,
    REGISTER_SIZE_64
};


enum register_type {
    REGISTER_TYPE_CALLEE_SAVED,
    REGISTER_TYPE_CALLER_SAVED,
    REGISTER_TYPE_RESERVED,
    REGISTER_TYPE_UNDEFINED
};

struct register_t {
    struct vector_t *names;
    struct IR_Operand *current_vreg;

    enum register_type type;
    enum register_size size;
    enum register_bank bank;

    int id;
    int arg_index;
    bool is_arg_reg;
    bool is_ret_reg;

    bool is_busy;
    bool is_reserved; // is reserved by compiler
};

struct register_list_t {
    struct register_t *registers;
    struct codegen_build_target_t *arch;

    int register_count;
};

struct codegen_context_t {
    struct codegen_t *codegen;
    FILE *file;
    struct IR_Function *current_function;
    struct IR_Function *main_function;
    struct codegen_build_target_t *build_target;
    struct vector_t *stack_slot_names;            // char *
};

struct clobber_t {
    struct register_t *reg;
    int time;
};

struct codegen_build_target_t{
    char *name;
    struct register_list_t *registers;
    int argument_register_count;      // for examle argument_register_count=6 in x86_64_linux

    const char *(*get_register_name) (struct register_list_t *list, struct register_t *reg, enum register_size size);
    struct register_t *(*get_best_available_register) (struct register_list_t *list, struct register_t *preferred_register, struct vector_t *clobber_list, struct IR_Operand *vreg);
    struct register_t *(*get_fixed_register_for_instruction) (struct register_list_t *list, struct IR_Instruction *instruction, struct IR_Operand *target_operand);
    void (*collect_instruction_clobbers) (struct register_list_t *list, struct IR_Instruction *instruction, struct vector_t *clobber_list, int time);

    void (*emit_globals)           (struct codegen_context_t *context, bool jmp_to_main);
    void (*emit_jmp_main)          (struct codegen_context_t *context);
    void (*emit_label)             (struct codegen_context_t *context, const char *label, bool is_global);
    void (*emit_function_prologue) (struct codegen_context_t *context, struct IR_Function *function);
    void (*emit_function_epilogue) (struct codegen_context_t *context, struct IR_Function *function);
    void (*emit_instruction)       (struct codegen_context_t *context, struct IR_Instruction *instruction);
    struct register_t *(*get_reg_with_arg_index) (struct register_list_t *list, int arg_index);


    void (*set_free_reserved_register)                   (struct register_list_t *list, struct register_t *reg);
    struct register_t *(*get_available_reserved_register)(struct register_list_t *list);
    struct register_t *(*ensure_operand_is_register)     (struct codegen_context_t *context, struct IR_Operand *operand);


    void (*emit_mov_reg_to_operand)    (struct codegen_context_t *context, struct IR_Operand *dest, struct register_t *src);
    void (*emit_mov_operand_to_reg)    (struct codegen_context_t *context, struct register_t *dest, struct IR_Operand *src);
    void (*emit_mov_reg_to_reg)        (struct codegen_context_t *context, struct register_t *dest, struct register_t *src, enum register_size size);
    void (*emit_mov_operand_to_operand)(struct codegen_context_t *context, struct IR_Operand *dest, struct IR_Operand *src);
};

void codegen_emit(FILE *file, const char *format,...);

struct codegen_t {
    struct vector_t *build_targets; // struct codegen_build_target_t *
    struct codegen_build_target_t *current_build_target;
    char *build_path;
};

struct codegen_t *codegen_create_codegen(char *build_path);
void codegen_delete_codegen(struct codegen_t **codegen);

struct codegen_build_target_t *codegen_create_build_target();
void codegen_delete_build_target(struct codegen_build_target_t **target);

struct register_list_t *codegen_create_register_list(struct codegen_build_target_t *arch, int register_count);
void codegen_delete_register_list(struct register_list_t **list);

struct register_t *codegen_init_register(struct register_t *reg, int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, bool is_reserved);
void codegen_delete_register(struct register_t *reg);

struct register_t *codegen_create_register_and_add_to_list(int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, bool is_reserved, struct register_list_t *list);

void codegen_build_project(struct codegen_t *codegen, struct IR_Project *project, char *build_target_name);
void codegen_build_module(struct codegen_t *codegen, struct IR_Module *module);
void codegen_build_function(struct codegen_context_t *context, struct IR_Function *function);

static inline void codegen_init_build_targets(struct codegen_t *codegen) {
    struct codegen_build_target_t *x86_64_linux = x86_64_linux_create_build_target();
    vector_add(codegen->build_targets, &x86_64_linux);
}

static inline void codegen_register_add_name(struct register_t *reg ,char *name) {
    vector_add(reg->names, &name);
}

static inline enum register_size codegen_calc_register_size(size_t byte) {
    switch (byte) {
        case 1: return REGISTER_SIZE_8;
        case 2: return REGISTER_SIZE_16;
        case 4: return REGISTER_SIZE_32;
        case 8: return REGISTER_SIZE_64;
    }
    return REGISTER_SIZE_64;
}

enum register_size codegen_get_register_size_from_operand(struct IR_Operand *operand);
int codegen_compare_register_sizes(enum register_size size_1, enum register_size size_2);

bool codegen_is_register_clobbered_for_vreg(struct register_t *reg, struct IR_Operand *vreg, struct vector_t *clobber_list);

static inline size_t codegen_padding(size_t size, size_t align){
    return (size + align) & ~align;
}

#endif
