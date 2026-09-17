#include "backend/x86_64/x86_64_macos.h"
#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_arena.h"
#include <stdbool.h>
#include <stddef.h>


#include "backend/x86_64/x86_64.h"
#include "h_string_view.h"

struct codegen_build_target_t *x86_64_macos_create_build_target(struct arena *arena) {
    struct codegen_build_target_t *target = codegen_create_build_target(arena);

    target->name = "x86_64_macos";
    target->registers = x86_64_create_register_list(arena, target);
    target->get_register_name = &x86_64_get_register_name;
    target->get_reg_in_reg_preference_order = &x86_64_get_reg_in_reg_preference_order;
    target->get_best_available_register = &x86_64_get_best_available_register;
    target->get_preferred_registers = &x86_64_get_preferred_registers;
    target->collect_instruction_clobbers = &x86_64_collect_instruction_clobbers;


    target->emit_globals = &x86_64_macos_emit_globals;
    target->emit_jmp_main = &x86_64_macos_emit_jmp_main;
    target->emit_function_epilogue = &x86_64_emit_function_epilogue;
    target->emit_function_prologue = &x86_64_emit_function_prologue;
    target->emit_instruction = &x86_64_emit_instruction;
    target->emit_label = &x86_64_macos_emit_label;
    target->get_label = &x86_64_macos_get_label_str;

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

void x86_64_macos_emit_jmp_main(struct codegen_context_t *context) {
    codegen_emit(context->file, "    xor rbp, rbp\n");
    codegen_emit(context->file, "    call " SV_FMT "\n", SV_ARG(context->build_target->get_label(context, context->main_function->mangled_name, true)));
    codegen_emit(context->file, "    mov rdi, rax\n");
    codegen_emit(context->file, "    mov rax, 0x2000001\n");
    codegen_emit(context->file, "    syscall\n");
}

void x86_64_macos_emit_globals(struct codegen_context_t *context, bool jmp_to_main) {
   codegen_emit(context->file, ".intel_syntax noprefix\n");
   codegen_emit(context->file, ".extern _exit\n");

   struct vector_t *globals = context->current_module->globals;
   int current_section = -1;

   for(int i = 0; i < globals->element_count; ++i) {
       struct IR_Operand *global = *(struct IR_Operand **) vector_get(globals, i);
       if(global->type == IR_OPERAND_TYPE_UNDEFINED) continue;

       if (global->data.global.kind == IR_GLOBAL_KIND_STRING && current_section != 1) {
           codegen_emit(context->file, ".section .rodata\n");
           current_section = 1;
       } else if (current_section != 0) {
           codegen_emit(context->file, ".data\n");
           current_section = 0;
       }

       codegen_emit(context->file, "    " SV_FMT ": ", SV_ARG(global->data.global.name));

       x86_64_emit_global_type(context, global);
       if(global->data.global.kind == IR_GLOBAL_KIND_STRING) {
           codegen_emit(context->file, " \"" SV_FMT "\"\n", SV_ARG(global->data.global.value));
       }else if(global->data.global.kind == IR_GLOBAL_KIND_VARIABLE) {
           codegen_emit(context->file, " " SV_FMT "\n", SV_ARG(global->data.global.value));
       }else if(global->data.global.kind == IR_GLOBAL_KIND_CONSTANT) {
           codegen_emit(context->file, " " SV_FMT "\n", SV_ARG(global->data.global.value));
       }
   }

   codegen_emit(context->file, ".text\n");
   codegen_emit(context->file, ".global _main\n");
   x86_64_macos_emit_label(context, SV("main"), true);
   if(jmp_to_main) x86_64_macos_emit_jmp_main(context);
}

void x86_64_macos_emit_label(struct codegen_context_t *context, const struct str_view label, bool is_global) {
    if(is_global) {
        codegen_emit(context->file, "_"SV_FMT ":\n", SV_ARG(label));
        return;
    }
    codegen_emit(context->file, "." SV_FMT ":\n", SV_ARG(label));
}

struct str_view x86_64_macos_get_label_str(struct codegen_context_t *context, const struct str_view label, bool is_global) {
    return str_view_fmt(context->codegen->arena, "_"SV_FMT, SV_ARG(label));
}
