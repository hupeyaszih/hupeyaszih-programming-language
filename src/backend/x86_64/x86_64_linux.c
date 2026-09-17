#include "backend/x86_64/x86_64_linux.h"
#include "backend/codegen.h"
#include "h_arena.h"
#include <stdbool.h>
#include <stddef.h>

#include "backend/x86_64/x86_64.h"

struct codegen_build_target_t *x86_64_linux_create_build_target(struct arena *arena) {
    struct codegen_build_target_t *target = codegen_create_build_target(arena);

    target->name = "x86_64_linux";
    target->registers = x86_64_create_register_list(arena, target);
    target->get_register_name = &x86_64_get_register_name;
    target->get_reg_in_reg_preference_order = &x86_64_get_reg_in_reg_preference_order;
    target->get_best_available_register = &x86_64_get_best_available_register;
    target->get_preferred_registers = &x86_64_get_preferred_registers;
    target->collect_instruction_clobbers = &x86_64_collect_instruction_clobbers;


    target->emit_globals = &x86_64_emit_globals;
    target->emit_jmp_main = &x86_64_emit_jmp_main;
    target->emit_function_epilogue = &x86_64_emit_function_epilogue;
    target->emit_function_prologue = &x86_64_emit_function_prologue;
    target->emit_instruction = &x86_64_emit_instruction;
    target->emit_label = &x86_64_emit_label;
    target->get_label = &x86_64_get_label_str;

    target->get_reg_with_arg_index = &x86_64_get_reg_with_arg_index;

    target->emit_mov_operand_to_operand = &x86_64_emit_mov_operand_to_operand;
    target->emit_mov_reg_to_operand     = &x86_64_emit_mov_reg_to_operand;
    target->emit_mov_operand_to_reg     = &x86_64_emit_mov_operand_to_reg;
    target->emit_mov_reg_to_reg         = &x86_64_emit_mov_reg_to_reg;

    target->get_available_reserved_register = &x86_64_get_available_reserved_register;
    target->set_free_reserved_register = &x86_64_set_free_reserved_register;
    target->ensure_operand_is_register = &x86_64_ensure_operand_is_register;

    target->argument_register_count = 6; // rdi, rsi, rdx, rcx, r8, r9
    return target;
}
