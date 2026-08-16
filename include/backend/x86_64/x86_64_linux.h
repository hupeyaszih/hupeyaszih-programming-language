#ifndef BACKEND_X86_64_LINUX_H
#define BACKEND_X86_64_LINUX_H


#include "h_arena.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stdio.h>
#define X86_64_RAX (0)
#define X86_64_RDI (1)
#define X86_64_RSI (2)
#define X86_64_RDX (3)
#define X86_64_RCX (4)
#define X86_64_R8  (5)
#define X86_64_R9  (6)
#define X86_64_R10 (7)
#define X86_64_R11 (8)

#define X86_64_RSP (9)
#define X86_64_RBP (10)

#define X86_64_RBX (11)
#define X86_64_R12 (12)
#define X86_64_R13 (13)
#define X86_64_R14 (14)
#define X86_64_R15 (15)
#define X86_64_REGISTER_COUNT (16)


#include <stdbool.h>

struct codegen_context_t;

struct register_t;
struct IR_Function;
struct IR_Instruction;
struct IR_Operand;

enum register_size;
enum IR_Instruction_type;



struct codegen_build_target_t *x86_64_linux_create_build_target(struct arena *arena);

struct register_list_t *x86_64_linux_create_register_list(struct arena *arena, struct codegen_build_target_t *arch);

const char *x86_64_linux_get_register_name(struct register_list_t *list, struct register_t *reg, enum register_size size);
int x86_64_linux_get_reg_in_reg_preference_order(int index);
struct register_t *x86_64_linux_get_best_available_register(struct register_list_t *list, struct register_t *preferred_register, struct vector_t *clobber_list, struct IR_Operand *vreg);

struct register_t *x86_64_linux_get_fixed_register_for_instruction(struct register_list_t *list, struct IR_Instruction *instruction, struct IR_Operand *target_operand);
void x86_64_collect_instruction_clobbers(struct register_list_t *list, struct IR_Instruction *instruction, struct vector_t *clobber_list, int time);


void x86_64_linux_emit_globals(struct codegen_context_t *context, bool jmp_to_main);
void x86_64_linux_emit_jmp_main(struct codegen_context_t *context);
void x86_64_linux_emit_label(struct codegen_context_t *context, const struct str_view label, bool is_global);
void x86_64_linux_emit_function_prologue(struct codegen_context_t *context, struct IR_Function *function);
void x86_64_linux_emit_function_epilogue(struct codegen_context_t *context, struct IR_Function *function);
void x86_64_linux_emit_instruction(struct codegen_context_t *context, struct IR_Instruction *instruction);

void x86_64_linux_emit_reg(struct codegen_context_t *context, struct register_t *reg, enum register_size size, bool print_size);
void x86_64_linux_emit_operand(struct codegen_context_t *context, struct IR_Operand *op, enum register_size size, bool print_size);
void x86_64_linux_emit_reg_size(struct codegen_context_t *context, enum register_size size);

struct register_t *x86_64_linux_get_reg_with_arg_index(struct register_list_t *list, int arg_index);
struct register_t *x86_64_linux_ensure_operand_is_register(struct codegen_context_t *context, struct IR_Operand *operand);
struct register_t *x86_64_linux_get_available_reserved_register(struct register_list_t *list);
void x86_64_linux_set_free_reserved_register(struct register_list_t *list, struct register_t *reg);


void x86_64_linux_emit_mov_reg_to_operand(struct codegen_context_t *context, struct IR_Operand *dest, struct register_t *src);
void x86_64_linux_emit_mov_operand_to_reg(struct codegen_context_t *context, struct register_t *dest, struct IR_Operand *src);
void x86_64_linux_emit_mov_reg_to_reg(struct codegen_context_t *context, struct register_t *dest, struct register_t *src, enum register_size size);
void x86_64_linux_emit_mov_operand_to_operand(struct codegen_context_t *context, struct IR_Operand *dest, struct IR_Operand *src);
void x86_64_linux_emit_mov_arg_to_stack_offset(struct codegen_context_t *context, int offset, bool is_argument, struct IR_Operand *src);
#endif
