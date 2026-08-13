#include "backend/x86_64/x86_64_linux.h"
#include "backend/codegen.h"
#include "backend/codegen_utils.h"
#include "core/ir_gen.h"
#include "core/symbol_table.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct codegen_build_target_t *x86_64_linux_create_build_target() {
    struct codegen_build_target_t *target = codegen_create_build_target();

    target->name = "x86_64_linux";
    target->registers = x86_64_linux_create_register_list(target);
    target->get_register_name = &x86_64_linux_get_register_name;
    target->get_best_available_register = &x86_64_linux_get_best_available_register;
    target->get_fixed_register_for_instruction = &x86_64_linux_get_fixed_register_for_instruction;
    target->collect_instruction_clobbers = &x86_64_collect_instruction_clobbers;


    target->emit_globals = &x86_64_linux_emit_globals;
    target->emit_function_epilogue = &x86_64_linux_emit_function_epilogue;
    target->emit_function_prologue = &x86_64_linux_emit_function_prologue;
    target->emit_instruction = &x86_64_linux_emit_instruction;
    target->emit_label = &x86_64_linux_emit_label;

    target->get_reg_with_arg_index = &x86_64_linux_get_reg_with_arg_index;

    target->emit_mov_operand_to_operand = &x86_64_linux_emit_mov_operand_to_operand;
    target->emit_mov_reg_to_operand     = &x86_64_linux_emit_mov_reg_to_operand;
    target->emit_mov_operand_to_reg     = &x86_64_linux_emit_mov_operand_to_reg;
    target->emit_mov_reg_to_reg         = &x86_64_linux_emit_mov_reg_to_reg;

    target->get_available_reserved_register = &x86_64_linux_get_available_reserved_register;
    target->set_free_reserved_register = &x86_64_linux_set_free_reserved_register;
    target->ensure_operand_is_register = &x86_64_linux_ensure_operand_is_register;

    target->argument_register_count = 6; // rdi, rsi, rdx, rcx, r8, r9
    return target;
}

struct register_list_t *x86_64_linux_create_register_list(struct codegen_build_target_t *arch) {
    const int register_count = 16; // general purpose register count
    struct register_list_t *list = codegen_create_register_list(arch, register_count);
    arch->registers = list;

    struct register_t *rax = codegen_create_register_and_add_to_list(X86_64_RAX, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, true , false, list);

    struct register_t *rdi = codegen_create_register_and_add_to_list(X86_64_RDI, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  0, false, false, list);
    struct register_t *rsi = codegen_create_register_and_add_to_list(X86_64_RSI, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  1, false, false, list);
    struct register_t *rdx = codegen_create_register_and_add_to_list(X86_64_RDX, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  2, false, false, list);
    struct register_t *rcx = codegen_create_register_and_add_to_list(X86_64_RCX, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  3, false, false, list);
    struct register_t *r8  = codegen_create_register_and_add_to_list(X86_64_R8 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  4, false, false, list);
    struct register_t *r9  = codegen_create_register_and_add_to_list(X86_64_R9 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true ,  5, false, false, list);

    struct register_t *r10 = codegen_create_register_and_add_to_list(X86_64_R10, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, false, true, list);
    struct register_t *r11 = codegen_create_register_and_add_to_list(X86_64_R11, REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, false, true, list);

    struct register_t *rsp = codegen_create_register_and_add_to_list(X86_64_RSP, REGISTER_SIZE_64, REGISTER_TYPE_RESERVED    , REGISTER_BANK_GPR, false, -1, false, false, list);
    struct register_t *rbp = codegen_create_register_and_add_to_list(X86_64_RBP, REGISTER_SIZE_64, REGISTER_TYPE_RESERVED    , REGISTER_BANK_GPR, false, -1, false, false, list);

    struct register_t *rbx = codegen_create_register_and_add_to_list(X86_64_RBX, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, false, list);
    struct register_t *r12 = codegen_create_register_and_add_to_list(X86_64_R12, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, false, list);
    struct register_t *r13 = codegen_create_register_and_add_to_list(X86_64_R13, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, false, list);
    struct register_t *r14 = codegen_create_register_and_add_to_list(X86_64_R14, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, false, list);
    struct register_t *r15 = codegen_create_register_and_add_to_list(X86_64_R15, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, false, list);

    codegen_register_add_name(rax ,"al");
    codegen_register_add_name(rax ,"ax");
    codegen_register_add_name(rax ,"eax");
    codegen_register_add_name(rax ,"rax");

    codegen_register_add_name(rdi ,"dil");
    codegen_register_add_name(rdi ,"di");
    codegen_register_add_name(rdi ,"edi");
    codegen_register_add_name(rdi ,"rdi");

    codegen_register_add_name(rsi ,"sil");
    codegen_register_add_name(rsi ,"si");
    codegen_register_add_name(rsi ,"esi");
    codegen_register_add_name(rsi ,"rsi");

    codegen_register_add_name(rdx ,"dl");
    codegen_register_add_name(rdx ,"dx");
    codegen_register_add_name(rdx ,"edx");
    codegen_register_add_name(rdx ,"rdx");

    codegen_register_add_name(rcx ,"cl");
    codegen_register_add_name(rcx ,"cx");
    codegen_register_add_name(rcx ,"ecx");
    codegen_register_add_name(rcx ,"rcx");

    codegen_register_add_name(r8 ,"r8b");
    codegen_register_add_name(r8 ,"r8w");
    codegen_register_add_name(r8 ,"r8d");
    codegen_register_add_name(r8 ,"r8");

    codegen_register_add_name(r9 ,"r9b");
    codegen_register_add_name(r9 ,"r9w");
    codegen_register_add_name(r9 ,"r9d");
    codegen_register_add_name(r9 ,"r9");

    codegen_register_add_name(r10 ,"r10b");
    codegen_register_add_name(r10 ,"r10w");
    codegen_register_add_name(r10 ,"r10d");
    codegen_register_add_name(r10 ,"r10");

    codegen_register_add_name(r11 ,"r11b");
    codegen_register_add_name(r11 ,"r11w");
    codegen_register_add_name(r11 ,"r11d");
    codegen_register_add_name(r11 ,"r11");

    codegen_register_add_name(rsp ,"spl");
    codegen_register_add_name(rsp ,"sp");
    codegen_register_add_name(rsp ,"esp");
    codegen_register_add_name(rsp ,"rsp");

    codegen_register_add_name(rbx ,"bl");
    codegen_register_add_name(rbx ,"bx");
    codegen_register_add_name(rbx ,"ebx");
    codegen_register_add_name(rbx ,"rbx");

    codegen_register_add_name(rbp ,"bpl");
    codegen_register_add_name(rbp ,"bp");
    codegen_register_add_name(rbp ,"ebp");
    codegen_register_add_name(rbp ,"rbp");

    codegen_register_add_name(r12 ,"r12b");
    codegen_register_add_name(r12 ,"r12w");
    codegen_register_add_name(r12 ,"r12d");
    codegen_register_add_name(r12 ,"r12");

    codegen_register_add_name(r13 ,"r13b");
    codegen_register_add_name(r13 ,"r13w");
    codegen_register_add_name(r13 ,"r13d");
    codegen_register_add_name(r13 ,"r13");

    codegen_register_add_name(r14 ,"r14b");
    codegen_register_add_name(r14 ,"r14w");
    codegen_register_add_name(r14 ,"r14d");
    codegen_register_add_name(r14 ,"r14");

    codegen_register_add_name(r15 ,"r15b");
    codegen_register_add_name(r15 ,"r15w");
    codegen_register_add_name(r15 ,"r15d");
    codegen_register_add_name(r15 ,"r15");


    return arch->registers;
}

struct register_t *x86_64_linux_get_reg_with_arg_index(struct register_list_t *list, int arg_index) {
    for(int i = 0;i < list->register_count; ++i) {
        struct register_t *reg = list->registers + i;
        if(arg_index == reg->arg_index) return reg;
    }
    return NULL;
}
const char *x86_64_linux_get_register_name(struct register_list_t *list, struct register_t *reg, enum register_size size) {
    return *(char **) vector_get(reg->names, size);
}

void x86_64_collect_instruction_clobbers(struct register_list_t *list, struct IR_Instruction *instruction, struct vector_t *clobber_list, int time) {
    if(!list || !clobber_list) return;
    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_DIVIDE: {
            struct clobber_t rax_clobber;
            rax_clobber.reg = list->registers + X86_64_RAX;
            rax_clobber.time = time;
            vector_add(clobber_list, &rax_clobber);

            struct clobber_t rdx_clobber;
            rdx_clobber.reg = list->registers + X86_64_RDX;
            rdx_clobber.time = time;
            vector_add(clobber_list, &rdx_clobber);
            break;
        }case IR_INSTRUCTION_TYPE_MUL: {
            enum register_size size = codegen_get_register_size_from_operand(instruction->operands.triple_operands.destination);
            if (REGISTER_SIZE_8 == size) {
                struct clobber_t rax_clobber;
                rax_clobber.reg = list->registers + X86_64_RAX;
                rax_clobber.time = time;
                vector_add(clobber_list, &rax_clobber);
            }
            break;
        }case IR_INSTRUCTION_TYPE_CALL: {
            struct clobber_t rax_clobber;
            rax_clobber.reg = list->registers + X86_64_RAX;
            rax_clobber.time = time;
            vector_add(clobber_list, &rax_clobber);
            break;
        }
    }
}

struct register_t *x86_64_linux_get_fixed_register_for_instruction(struct register_list_t *list, struct IR_Instruction *instruction, struct IR_Operand *target_operand) {
    if (!list) return NULL;


    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_DIVIDE: {
            if (target_operand == instruction->operands.triple_operands.source_1) {
                return list->registers + X86_64_RAX;
            }else if (target_operand == instruction->operands.triple_operands.destination) {
                return list->registers + X86_64_RAX;
            }else if (target_operand == instruction->operands.triple_operands.source_2) {
                return NULL;
            }
            return NULL;
        }case IR_INSTRUCTION_TYPE_MUL: {
            enum register_size size = codegen_calc_register_size(instruction->operands.triple_operands.destination->type_info->size);
            if (REGISTER_SIZE_8 == size) {
                if (target_operand == instruction->operands.triple_operands.source_1) return list->registers + X86_64_RAX;
                if (target_operand == instruction->operands.triple_operands.destination) return list->registers + X86_64_RAX;
            }
            return NULL;
        }case IR_INSTRUCTION_TYPE_CALL: {
            if(target_operand == instruction->operands.call.return_val) {
                return NULL;
            }
            int arg_index = IR_call_get_arg_index(instruction, target_operand);

            if (arg_index < 0 || arg_index >= 6) return NULL; 
            return x86_64_linux_get_reg_with_arg_index(list, arg_index);
        }
        case IR_INSTRUCTION_TYPE_RET: return list->registers+X86_64_RAX;

        default: return NULL;
    }
    return NULL;
}


static const int REG_PREFERENCE_ORDER[X86_64_REGISTER_COUNT] = {
    X86_64_R8,  X86_64_R9, X86_64_R10, X86_64_R11,  X86_64_RDI, X86_64_RSI,
    X86_64_RDX, X86_64_RCX, X86_64_RAX,
    X86_64_RBX, X86_64_R12, X86_64_R13, X86_64_R14, X86_64_R15
};

void x86_64_linux_set_free_reserved_register(struct register_list_t *list, struct register_t *reg) {
    if(!reg || !list) return;
    if(!reg->is_reserved) return;
    reg->is_busy = false;
}
struct register_t *x86_64_linux_get_available_reserved_register(struct register_list_t *list) {
    for(int i = 0;i < list->register_count; ++i) {
        struct register_t *reg = list->registers + REG_PREFERENCE_ORDER[i];
        if(true == reg->is_busy || false == reg->is_reserved) continue;
        reg->is_busy = true;

        return reg;
    }
    return NULL;
}

struct register_t *x86_64_linux_get_best_available_register(struct register_list_t *list, struct register_t *preferred_register, struct vector_t *clobber_list, struct IR_Operand *vreg) {
    if(NULL != preferred_register && !preferred_register->is_busy && !preferred_register->is_reserved) {
        if(!codegen_is_register_clobbered_for_vreg(preferred_register, vreg, clobber_list)) return preferred_register;
    }

    for(int i = 0;i < list->register_count; ++i) {
        struct register_t *reg = list->registers + REG_PREFERENCE_ORDER[i];
        if(true == reg->is_busy || true == reg->is_reserved || codegen_is_register_clobbered_for_vreg(reg, vreg, clobber_list)) continue;

        return reg;
    }

    return NULL;
}


// emit
static inline struct register_t *get_function_return_register(struct IR_Function *function, struct register_list_t *list) {
    struct register_t *rax = list->registers + X86_64_RAX;
    return rax;
}

static inline int check_stack_aligment_before_call(struct codegen_context_t *context, int *stack_size) {
    return *stack_size % 16;
}

void x86_64_linux_emit_mov_reg_to_reg(struct codegen_context_t *context, struct register_t *dest, struct register_t *src, enum register_size size) {
    const char *src_str = context->build_target->get_register_name(context->build_target->registers, src, size);
    const char *dest_str = context->build_target->get_register_name(context->build_target->registers, dest, size);

    codegen_emit(context->file, "    mov %s, %s   #reg to reg\n", dest_str, src_str);
}

void x86_64_linux_emit_mov_arg_to_stack_offset(struct codegen_context_t *context, int offset, bool is_argument, struct IR_Operand *src) {
    struct register_t *reg = x86_64_linux_ensure_operand_is_register(context, src);


    if(is_argument) {
        codegen_emit(context->file, "    mov [rsp + %d], ", offset);
    }else {
        codegen_emit(context->file, "    mov [rsp - %d], ", offset);
    }
    x86_64_linux_emit_reg(context, reg, REGISTER_SIZE_64, false);
    codegen_emit(context->file, "\n");


    x86_64_linux_set_free_reserved_register(context->build_target->registers, reg);
}

void x86_64_linux_emit_mov_operand_to_reg(struct codegen_context_t *context, struct register_t *dest, struct IR_Operand *src) {
    enum register_size src_size = codegen_get_register_size_from_operand(src);
    enum register_size dest_size = dest->size;

    if(IR_OPERAND_TYPE_STACK_SLOT == src->type) {
        codegen_emit(context->file, "    mov ");
        x86_64_linux_emit_reg(context, dest, src_size, true);
        codegen_emit(context->file, ", ");
        x86_64_linux_emit_operand(context, src, src_size, true);
        codegen_emit(context->file, "#op (stack) to reg\n");
        return;
    }else if(IR_OPERAND_TYPE_IMM == src->type) {
        codegen_emit(context->file, "    mov ");
        x86_64_linux_emit_reg(context, dest, dest_size, true);
        codegen_emit(context->file, ", ");
        x86_64_linux_emit_operand(context, src, dest_size, true);
        codegen_emit(context->file, "#op (imm) to reg\n");
        return;
    }else if(IR_OPERAND_TYPE_VREG == src->type) {
        x86_64_linux_emit_mov_reg_to_reg(context, dest, src->data.vreg.reg, src_size);
        return;
    }
}

void x86_64_linux_emit_mov_reg_to_operand(struct codegen_context_t *context, struct IR_Operand *dest, struct register_t *src) {
    enum register_size size = codegen_get_register_size_from_operand(dest);

    if(IR_OPERAND_TYPE_VREG == dest->type) {
        x86_64_linux_emit_mov_reg_to_reg(context, dest->data.vreg.reg, src, size);
        return;
    }else if(IR_OPERAND_TYPE_STACK_SLOT == dest->type) {
        codegen_emit(context->file, "    mov ");
        x86_64_linux_emit_operand(context, dest, size, true);
        codegen_emit(context->file, ", ");
        x86_64_linux_emit_reg(context, src, size, true);
        codegen_emit(context->file, "#reg to op (slot)\n");
        return;
    }else if(IR_OPERAND_TYPE_IMM == dest->type) {
        codegen_emit(context->file, "    mov ");
        x86_64_linux_emit_operand(context, dest, size, true);
        codegen_emit(context->file, ", ");
        x86_64_linux_emit_reg(context, src, size, true);
        codegen_emit(context->file, "#reg to op (imm)\n");
        return;
    }
}
void x86_64_linux_emit_mov_operand_to_operand(struct codegen_context_t *context, struct IR_Operand *dest, struct IR_Operand *src) {
    enum register_size dest_size = codegen_get_register_size_from_operand(dest);
    enum register_size src_size = codegen_get_register_size_from_operand(src);

    enum IR_Operand_type dest_type = dest->type;
    enum IR_Operand_type src_type = src->type;

    if(IR_OPERAND_TYPE_STACK_SLOT == dest->type && IR_OPERAND_TYPE_STACK_SLOT == src->type) {
        struct register_t *reserved_reg = x86_64_linux_get_available_reserved_register(context->build_target->registers);
        x86_64_linux_emit_mov_operand_to_reg(context, reserved_reg, src);
        x86_64_linux_emit_mov_reg_to_operand(context, dest, reserved_reg);
        x86_64_linux_set_free_reserved_register(context->build_target->registers, reserved_reg);
        return;
    }else if(IR_OPERAND_TYPE_STACK_SLOT == dest_type && IR_OPERAND_TYPE_IMM == src_type) {
        struct register_t *reserved_reg = x86_64_linux_get_available_reserved_register(context->build_target->registers);
        x86_64_linux_emit_mov_operand_to_reg(context, reserved_reg, src);
        x86_64_linux_emit_mov_reg_to_operand(context, dest, reserved_reg);
        x86_64_linux_set_free_reserved_register(context->build_target->registers, reserved_reg);
        return;
    }else if(IR_OPERAND_TYPE_STACK_SLOT == dest_type && IR_OPERAND_TYPE_VREG == src_type) {
        x86_64_linux_emit_mov_reg_to_operand(context, dest, src->data.vreg.reg);
        return;
    }else if(IR_OPERAND_TYPE_VREG == dest_type && IR_OPERAND_TYPE_VREG == src_type) {
        x86_64_linux_emit_mov_reg_to_reg(context, dest->data.vreg.reg, src->data.vreg.reg, src_size);
        return;
    }else if(IR_OPERAND_TYPE_VREG == dest_type && IR_OPERAND_TYPE_IMM == src_type) {
        x86_64_linux_emit_mov_operand_to_reg(context, dest->data.vreg.reg, src);
        return;
    }else if(IR_OPERAND_TYPE_VREG == dest_type && IR_OPERAND_TYPE_STACK_SLOT == src_type) {
        x86_64_linux_emit_mov_operand_to_reg(context, dest->data.vreg.reg, src);
        return;
    }
}

static inline int push_or_pop_registers(struct codegen_context_t *context, struct bitset_t *registers, struct bitset_t *registers2, struct bitset_t *mask, enum register_size size, const char *instruction, bool reverse, bool nop) {
    int pushed_or_popped_register_count = 0;
    if(reverse) {
        for(int i = registers->max_element_count; i >= 0; --i) {
            int is_caller_saved_used = bitset_test(registers, i);

            int is_caller_saved_used_in_caller_function = 1;
            int masked = 0;
            if(registers2) {
                is_caller_saved_used_in_caller_function = bitset_test(registers2, i);
            }
            if(mask)masked = bitset_test(mask, i);

            if(is_caller_saved_used == 0 || is_caller_saved_used_in_caller_function == 0 || masked == 1) continue;

            if(!nop) {
                const char *caller_saved_reg = x86_64_linux_get_register_name(context->build_target->registers, context->build_target->registers->registers + i, size);
                codegen_emit(context->file, "    %s %s\n", instruction , caller_saved_reg);
            }

            ++pushed_or_popped_register_count;
        }
        return pushed_or_popped_register_count;
    }
    for(int i = 0;i < registers->max_element_count; ++i) {
        int is_caller_saved_used = bitset_test(registers, i);

        int is_caller_saved_used_in_caller_function = 1;
        int masked = 0;
        if(registers2) {
            is_caller_saved_used_in_caller_function = bitset_test(registers2, i);
        }
        if(mask)masked = bitset_test(mask, i);

        if(is_caller_saved_used == 0 || is_caller_saved_used_in_caller_function == 0 || masked == 1) continue;

        if(!nop) {
            const char *caller_saved_reg = x86_64_linux_get_register_name(context->build_target->registers, context->build_target->registers->registers + i, size);
            codegen_emit(context->file, "    %s %s\n", instruction , caller_saved_reg);
        }

        ++pushed_or_popped_register_count;
    }
    return pushed_or_popped_register_count;
}

struct register_t *x86_64_linux_ensure_operand_is_register(struct codegen_context_t *context, struct IR_Operand *operand) {
    if(IR_OPERAND_TYPE_VREG == operand->type) return operand->data.vreg.reg;

    struct register_t *reserved_reg = x86_64_linux_get_available_reserved_register(context->build_target->registers);
    x86_64_linux_emit_mov_operand_to_reg(context, reserved_reg, operand);
    return reserved_reg;
}

void x86_64_linux_emit_globals(struct codegen_context_t *context, bool jmp_to_main) {
   codegen_emit(context->file, ".intel_syntax noprefix\n");
   codegen_emit(context->file, ".global _start\n");
   x86_64_linux_emit_label(context, "_start", true);
   if(jmp_to_main) x86_64_linux_emit_jmp_main(context);
}

void x86_64_linux_emit_jmp_main(struct codegen_context_t *context) {
    codegen_emit(context->file, "    xor rbp, rbp\n");
    codegen_emit(context->file, "    call %s\n",context->main_function->mangled_name);
    codegen_emit(context->file, "    mov rdi, rax\n");
    codegen_emit(context->file, "    mov rax, 60\n");
    codegen_emit(context->file, "    syscall\n\n");
}

void x86_64_linux_emit_label(struct codegen_context_t *context, const char *label, bool is_global) {
    if(is_global) {
        codegen_emit(context->file, "%s:\n", label);
        return;
    }
    codegen_emit(context->file, ".%s:\n", label);
}

void x86_64_linux_emit_function_prologue(struct codegen_context_t *context, struct IR_Function *function) {
    codegen_emit(context->file, ".global %s\n", function->mangled_name);
    x86_64_linux_emit_label(context, function->mangled_name, true);
    codegen_emit(context->file, "    push rbp\n");
    codegen_emit(context->file, "    mov rbp, rsp\n");
    codegen_emit(context->file, "    sub rsp, %ld\n", codegen_padding(function->stack_size, 7));

    int pushed_callee_saved_register_count = push_or_pop_registers(context, function->used_callee_saved_registers, NULL, NULL,REGISTER_SIZE_64, "push", false, false);
    int pushed_callee_saved_registers_size = pushed_callee_saved_register_count * 8;
    int aligned_stack_off = codegen_padding(pushed_callee_saved_registers_size, 15);
    int align = aligned_stack_off - pushed_callee_saved_registers_size;
    if(align != 0)codegen_emit(context->file, "    sub rsp, %d\n", align);

}

void x86_64_linux_emit_function_epilogue(struct codegen_context_t *context, struct IR_Function *function) {
    int pushed_callee_saved_register_count = push_or_pop_registers(context, function->used_callee_saved_registers, NULL, NULL,REGISTER_SIZE_64, "pop", true, true);
    int pushed_callee_saved_registers_size = pushed_callee_saved_register_count * 8;
    int aligned_stack_off = codegen_padding(pushed_callee_saved_registers_size, 15);
    int align = aligned_stack_off - pushed_callee_saved_registers_size;
    if(align != 0) codegen_emit(context->file, "    add rsp, %d\n", align);
    push_or_pop_registers(context, function->used_callee_saved_registers, NULL, NULL,REGISTER_SIZE_64, "pop", true, false);

    codegen_emit(context->file, "    mov rsp, rbp\n");
    codegen_emit(context->file, "    pop rbp\n");
    codegen_emit(context->file, "    ret\n\n");
}



void emit_cast_instruction(struct codegen_context_t *context, struct IR_Instruction *instruction) {
    struct IR_Operand *src = instruction->operands.double_operands.source_1;
    struct IR_Operand *dest = instruction->operands.double_operands.destination;
    struct register_t *src_reg = x86_64_linux_ensure_operand_is_register(context, src);

    enum register_size src_size = codegen_get_register_size_from_operand(src);
    enum register_size dest_size = codegen_get_register_size_from_operand(dest);

    if(dest_size > src_size) {
        codegen_emit(context->file, "    movsx ");
        x86_64_linux_emit_operand(context, dest, dest_size, false);
        codegen_emit(context->file, ", ");
        x86_64_linux_emit_reg(context, src_reg, src_size, false);
        codegen_emit(context->file, "\n");
        x86_64_linux_set_free_reserved_register(context->build_target->registers, src_reg);
    }else if(dest_size < src_size) {
        return;
    } else {
        return;
    }
}


void emit_comparison_instructions(struct codegen_context_t *context, struct IR_Instruction *instruction) {
    char *instruction_str = NULL;
    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:     instruction_str = "sete";  break;
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:      instruction_str = "setne"; break;
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:      instruction_str = "setle"; break;
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:   instruction_str = "setge"; break;
        case IR_INSTRUCTION_TYPE_LESS:            instruction_str = "setl";  break;
        case IR_INSTRUCTION_TYPE_GREATER:         instruction_str = "setg";  break;
    }

    struct IR_Operand *dest = instruction->operands.triple_operands.destination;
    struct IR_Operand *src1 = instruction->operands.triple_operands.source_1;
    struct IR_Operand *src2 = instruction->operands.triple_operands.source_2;

    struct register_t *src1_reg = x86_64_linux_ensure_operand_is_register(context, src1);
    
    struct register_t *dest_reg = x86_64_linux_ensure_operand_is_register(context, dest);

    enum register_size size = codegen_get_register_size_from_operand(src1);

    codegen_emit(context->file, "    cmp ");
    x86_64_linux_emit_reg(context, src1_reg, size, true);
    codegen_emit(context->file, ", ");
    x86_64_linux_emit_operand(context, src2, size, true);
    codegen_emit(context->file, "\n");

    codegen_emit(context->file, "    %s ", instruction_str);
    x86_64_linux_emit_reg(context, dest_reg, REGISTER_SIZE_8, true);
    codegen_emit(context->file, "\n");

    codegen_emit(context->file, "    movzx ");
    x86_64_linux_emit_reg(context, dest_reg, REGISTER_SIZE_64, true);
    codegen_emit(context->file, ", ");
    x86_64_linux_emit_reg(context, dest_reg, REGISTER_SIZE_8, true);
    codegen_emit(context->file, "\n");
}

void emit_arithmetic_instructions(struct codegen_context_t *context, struct IR_Instruction *instruction) {
    char *instruction_str = NULL;
    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_PLUS:   instruction_str = "add";  break;
        case IR_INSTRUCTION_TYPE_MINUS:  instruction_str = "sub";  break;
        case IR_INSTRUCTION_TYPE_MUL:    instruction_str = "imul"; break;
        case IR_INSTRUCTION_TYPE_DIVIDE: instruction_str = "idiv"; break;
    }

    struct IR_Operand *dest = instruction->operands.triple_operands.destination;
    struct IR_Operand *src1 = instruction->operands.triple_operands.source_1;
    struct IR_Operand *src2 = instruction->operands.triple_operands.source_2;
    enum IR_Operand_type dest_type = dest->type;
    enum IR_Operand_type src1_type = src1->type;
    enum IR_Operand_type src2_type = src2->type;



    enum register_size size = codegen_get_register_size_from_operand(dest);

    if(IR_INSTRUCTION_TYPE_MUL == instruction->type) {
        if(REGISTER_SIZE_8 == size) {  // imul source_2
            struct register_t *rax = context->build_target->registers->registers + X86_64_RAX;

            struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);
            codegen_emit(context->file, "    %s ", instruction_str);
            x86_64_linux_emit_reg(context, src2_reg, size, false);
            codegen_emit(context->file, "    \n");

            x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);

            x86_64_linux_emit_mov_reg_to_operand(context, dest, rax);

            goto exit;
        }

        if(dest == src1) { // imm dest, src2
            if(IR_OPERAND_TYPE_STACK_SLOT == dest_type) {
                struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);

                codegen_emit(context->file, "    %s ", instruction_str);
                x86_64_linux_emit_operand(context, dest, size, false);
                codegen_emit(context->file, ", ", instruction_str);
                x86_64_linux_emit_reg(context, src2_reg, size, false);
                codegen_emit(context->file, "\n", instruction_str);

                x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);
                goto exit;
            }

            codegen_emit(context->file, "    %s ", instruction_str);
            x86_64_linux_emit_operand(context, dest, size, false);
            codegen_emit(context->file, ", ", instruction_str);
            x86_64_linux_emit_operand(context, src2, size, false);
            codegen_emit(context->file, "\n", instruction_str);
            goto exit;
        }

        if(IR_OPERAND_TYPE_VREG == dest_type && (IR_OPERAND_TYPE_VREG == src1_type || IR_OPERAND_TYPE_STACK_SLOT == src1_type) && (IR_OPERAND_TYPE_IMM == src2_type && src2->type_info->size <= 4)) { // imm dest, src1, src2
            struct register_t *dest_reg = x86_64_linux_ensure_operand_is_register(context, dest);

            codegen_emit(context->file, "    %s ", instruction_str);
            x86_64_linux_emit_reg(context, dest_reg, size, false);
            codegen_emit(context->file, ", ", instruction_str);
            x86_64_linux_emit_operand(context, src1, size, false);
            codegen_emit(context->file, ", ", instruction_str);
            x86_64_linux_emit_operand(context, src2, size, false);
            codegen_emit(context->file, "\n", instruction_str);

            x86_64_linux_set_free_reserved_register(context->build_target->registers, dest_reg);
            goto exit;
        }

        // mov dest, src1
        // imul dest, src2
        struct register_t *dest_reg = x86_64_linux_ensure_operand_is_register(context, dest);
        struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);

        x86_64_linux_emit_mov_operand_to_reg(context, dest_reg, src1);
        codegen_emit(context->file, "    %s ", instruction_str);
        x86_64_linux_emit_reg(context, dest_reg, size, false);
        codegen_emit(context->file, ", ", instruction_str);
        x86_64_linux_emit_reg(context, src2_reg, size, false);
        codegen_emit(context->file, "\n", instruction_str);

        x86_64_linux_emit_mov_reg_to_operand(context, dest, dest_reg);
        

        x86_64_linux_set_free_reserved_register(context->build_target->registers, dest_reg);
        x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);
        goto exit;
    }else if(IR_INSTRUCTION_TYPE_DIVIDE == instruction->type) {

        struct register_t *rax = context->build_target->registers->registers + X86_64_RAX;
        if(dest != src1) {
            x86_64_linux_emit_mov_operand_to_operand(context, dest, src1);
        }
        x86_64_linux_emit_mov_operand_to_reg(context, rax, dest);

        char *cb = NULL;
        switch (size) {
            case REGISTER_SIZE_8:  cb = "cbw"; break;
            case REGISTER_SIZE_16: cb = "cwd"; break;
            case REGISTER_SIZE_32: cb = "cdq"; break;
            case REGISTER_SIZE_64: cb = "cqo"; break;
        }
        codegen_emit(context->file, "    %s\n", cb);

        struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);


        codegen_emit(context->file, "    idiv ");
        x86_64_linux_emit_reg(context, src2_reg, size, false);
        codegen_emit(context->file, "\n");

        x86_64_linux_emit_mov_reg_to_operand(context, dest, rax);
        x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);
        goto exit;
    }

    // add/sub

    if(dest == src1) {
        if(IR_OPERAND_TYPE_STACK_SLOT == dest_type) {
            struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);

            codegen_emit(context->file, "    %s ", instruction_str);
            x86_64_linux_emit_operand(context, dest, size, false);
            codegen_emit(context->file, ", ", instruction_str);
            x86_64_linux_emit_reg(context, src2_reg, size, false);
            codegen_emit(context->file, "\n", instruction_str);

            x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);
            goto exit;
        }
        codegen_emit(context->file, "    %s ", instruction_str);
        x86_64_linux_emit_operand(context, dest, size, false);
        codegen_emit(context->file, ", ", instruction_str);
        x86_64_linux_emit_operand(context, src2, size, false);
        codegen_emit(context->file, "\n", instruction_str);
        goto exit;
    }

    if(IR_OPERAND_TYPE_STACK_SLOT == dest_type) {
        x86_64_linux_emit_mov_operand_to_operand(context, dest, src1);
        struct register_t *src2_reg = x86_64_linux_ensure_operand_is_register(context, src2);

        codegen_emit(context->file, "    %s ", instruction_str);
        x86_64_linux_emit_operand(context, dest, size, false);
        codegen_emit(context->file, ", ", instruction_str);
        x86_64_linux_emit_reg(context, src2_reg, size, false);
        codegen_emit(context->file, "\n", instruction_str);

        x86_64_linux_set_free_reserved_register(context->build_target->registers, src2_reg);
        goto exit;
    }

    x86_64_linux_emit_mov_operand_to_operand(context, dest, src1);
    codegen_emit(context->file, "    %s ", instruction_str);
    x86_64_linux_emit_operand(context, dest, size, false);
    codegen_emit(context->file, ", ", instruction_str);
    x86_64_linux_emit_operand(context, src2, size, false);
    codegen_emit(context->file, "\n", instruction_str);
exit:
    return;
}

void x86_64_linux_emit_instruction(struct codegen_context_t *context, struct IR_Instruction *instruction) {
    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_LOAD:
        case IR_INSTRUCTION_TYPE_MOV: {
            x86_64_linux_emit_mov_operand_to_operand(context, instruction->operands.double_operands.destination, instruction->operands.double_operands.source_1);
            break;
        }case IR_INSTRUCTION_TYPE_STORE: {
            struct IR_Operand *src = instruction->operands.double_operands.source_1;
            struct IR_Operand *dest = instruction->operands.double_operands.destination;
            if(IR_OPERAND_TYPE_STACK_SLOT == dest->type) {
                x86_64_linux_emit_mov_operand_to_operand(context, dest, src);
                codegen_emit(context->file, "\n");
                break;
            }

            struct register_t *src_reg = x86_64_linux_ensure_operand_is_register(context, src);
            enum register_size size = codegen_get_register_size_from_operand(src);
            // mov [dest], src
            codegen_emit(context->file, "    mov [ ");
            x86_64_linux_emit_operand(context, dest, REGISTER_SIZE_64, true);
            codegen_emit(context->file, "], ");
            x86_64_linux_emit_reg(context, src_reg, size, true);
            codegen_emit(context->file, "\n");
            x86_64_linux_set_free_reserved_register(context->build_target->registers, src_reg);
            break; 
        }
        case IR_INSTRUCTION_TYPE_PLUS: emit_arithmetic_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_MINUS: emit_arithmetic_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_MUL: emit_arithmetic_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_DIVIDE: emit_arithmetic_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_RET: {
            struct register_t *rax = context->build_target->registers->registers + X86_64_RAX;
            // enum register_size size = codegen_calc_register_size(instruction->operands.ret.return_value->type_info->size);
            enum register_size size = codegen_get_register_size_from_operand(instruction->operands.ret.return_value);
            const char *dest_str = context->build_target->get_register_name(context->build_target->registers, rax, size);
            codegen_emit(context->file, "    mov %s, ", dest_str);
            x86_64_linux_emit_operand(context, instruction->operands.ret.return_value, size, false);
            codegen_emit(context->file, "    \n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_MINUS: {
            struct IR_Operand *dest = instruction->operands.double_operands.destination;
            struct IR_Operand *src = instruction->operands.double_operands.source_1;

            enum register_size size = codegen_get_register_size_from_operand(dest);

            if(IR_OPERAND_TYPE_IMM == src->type) {
                struct register_t *reserved_reg = x86_64_linux_get_available_reserved_register(context->build_target->registers);

                x86_64_linux_emit_mov_operand_to_reg(context, reserved_reg, src);

                codegen_emit(context->file, "    neg ");
                x86_64_linux_emit_reg(context, reserved_reg, size, true);
                codegen_emit(context->file, "\n");
                x86_64_linux_emit_mov_reg_to_operand(context, dest, reserved_reg);

                x86_64_linux_set_free_reserved_register(context->build_target->registers, reserved_reg);
            }else {
                codegen_emit(context->file, "    neg ");
                x86_64_linux_emit_operand(context, src, size, true);
                codegen_emit(context->file, "\n");
                x86_64_linux_emit_mov_operand_to_operand(context, dest, src);
            }

            break;
        }case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF: {
            struct IR_Operand *dest = instruction->operands.double_operands.destination;
            struct IR_Operand *src = instruction->operands.double_operands.source_1;

            enum register_size src_size = codegen_get_register_size_from_operand(src);

            struct register_t *dest_reg = x86_64_linux_ensure_operand_is_register(context, dest);

            codegen_emit(context->file, "    lea ");
            x86_64_linux_emit_reg(context, dest_reg, REGISTER_SIZE_64, false);
            codegen_emit(context->file, ", ");
            x86_64_linux_emit_operand(context, src, src_size, false);
            codegen_emit(context->file, "\n");

            x86_64_linux_set_free_reserved_register(context->build_target->registers, dest_reg);
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            struct IR_Operand *dest = instruction->operands.double_operands.destination;
            struct IR_Operand *src = instruction->operands.double_operands.source_1;

            codegen_emit(context->file, "\n");
            enum register_size size = codegen_get_register_size_from_operand(dest);

            struct register_t *src_reg = x86_64_linux_ensure_operand_is_register(context, src);

            struct register_t *val_reg = x86_64_linux_get_available_reserved_register(context->build_target->registers);

            codegen_emit(context->file, "    mov ");
            x86_64_linux_emit_reg(context, val_reg, size, true);
            codegen_emit(context->file, ", [ ");
            x86_64_linux_emit_reg(context, src_reg, REGISTER_SIZE_64, true);
            codegen_emit(context->file, " ]\n");

            x86_64_linux_emit_mov_reg_to_operand(context, dest, val_reg);

            x86_64_linux_set_free_reserved_register(context->build_target->registers, val_reg);
            x86_64_linux_set_free_reserved_register(context->build_target->registers, src_reg);
            break;
        }case IR_INSTRUCTION_TYPE_CALL: {
            struct IR_Function *func = instruction->operands.call.target_function;
            struct IR_Operand *return_val = instruction->operands.call.return_val;
            struct IR_Function *caller_func = instruction->parent_block->parent_function;
            int arg_count = instruction->operands.call.arguments->element_count;

            struct bitset_t *mask = bitset_create(context->build_target->registers->register_count);
            bitset_set(mask, X86_64_RAX);
            // push caller saved registers
            int pushed_caller_saved_register_count = push_or_pop_registers(context, func->used_caller_saved_registers, caller_func->directly_used_caller_saved_registers, mask, REGISTER_SIZE_64, "push", false, false);
            //

            int args_via_registers = arg_count;
            if(arg_count > 6) args_via_registers = context->build_target->argument_register_count;
            int args_via_stack = (arg_count > 6) ? (arg_count - 6) : 0;
            int stack_args_bytes = args_via_stack * 8;
            int pushed_regs_bytes = pushed_caller_saved_register_count * 8;

            int total_stack_delta = pushed_regs_bytes + stack_args_bytes;
            int aligned_stack_off = codegen_padding(total_stack_delta, 15);
            int align_padding = aligned_stack_off - total_stack_delta;
            int total_rsp_sub = stack_args_bytes + align_padding;

            if (total_rsp_sub > 0) codegen_emit(context->file, "    sub rsp, %d\n", total_rsp_sub);

            for (int i = arg_count - 1; i >= 6; --i) {
                struct IR_Operand *arg = *(struct IR_Operand **) vector_get(instruction->operands.call.arguments, i);
                int stack_index = i - 6;
                int offset = stack_index * 8;

                x86_64_linux_emit_mov_arg_to_stack_offset(context, offset, true, arg);
            }


            struct vector_t *out_regs = vector_create_vector(args_via_registers, sizeof(struct register_t *));
            for(int i = 0;i < args_via_registers; ++i) {
                struct register_t *reg = x86_64_linux_get_reg_with_arg_index(context->build_target->registers, i);
                vector_add(out_regs, &reg);
            }
            codegen_utils_emit_call_args(context, instruction->operands.call.arguments, out_regs, args_via_registers);
            vector_free(&out_regs);

            codegen_emit(context->file, "    call %s\n", func->mangled_name);

            if (total_rsp_sub > 0) codegen_emit(context->file, "    add rsp, %d\n", total_rsp_sub);


            // pop caller saved registers
            push_or_pop_registers(context, func->used_caller_saved_registers, caller_func->directly_used_caller_saved_registers, mask,REGISTER_SIZE_64, "pop", true, false);
            //

            struct register_t *rax = context->build_target->registers->registers + X86_64_RAX;
            x86_64_linux_emit_mov_reg_to_operand(context, return_val, rax);
            bitset_free(&mask);
            break;
        }case IR_INSTRUCTION_TYPE_JMP: {
            struct vector_t *args = instruction->operands.jmp.args;
            struct IR_Block *target_block = instruction->operands.jmp.target_block;
            if(args) {

                int arg_count = args->element_count;


                struct vector_t *out_regs = vector_create_vector(arg_count, sizeof(struct register_t *));
                for(int i = 0;i < arg_count; ++i) {
                    struct IR_Operand *op = *(struct IR_Operand **)vector_get(target_block->params, i);
                    vector_add(out_regs, &(op->data.vreg.reg));
                }
                codegen_utils_emit_call_args(context, args, out_regs, arg_count);
                vector_free(&out_regs);
            }

            codegen_emit(context->file, "    jmp .%s\n", target_block->mangled_name);
            break;
        }case IR_INSTRUCTION_TYPE_BR: {
            struct IR_Operand *condition = instruction->operands.br.condition;
            {   // false block
                struct vector_t *args = instruction->operands.br.false_args;
                struct IR_Block *target_block = instruction->operands.br.false_block;


                codegen_emit(context->file, "    cmp ");
                x86_64_linux_emit_operand(context, condition, codegen_get_register_size_from_operand(condition), true);
                codegen_emit(context->file, ", 0\n");

                if(args) {
                    int arg_count = args->element_count;
                    struct vector_t *out_regs = vector_create_vector(arg_count, sizeof(struct register_t *));
                    for(int i = 0;i < arg_count; ++i) {
                        struct IR_Operand *op = *(struct IR_Operand **)vector_get(target_block->params, i);
                        vector_add(out_regs, &(op->data.vreg.reg));
                    }
                    codegen_utils_emit_call_args(context, args, out_regs, arg_count);
                    vector_free(&out_regs);
                }

                codegen_emit(context->file, "    je .%s\n", target_block->mangled_name);
            }
            break;
        }case IR_INSTRUCTION_TYPE_CAST: {
            emit_cast_instruction(context, instruction);
            break;
        }case IR_INSTRUCTION_TYPE_ASM: {
            char *asm_imm = instruction->operands.asm_operands.asm_imm;
            codegen_emit(context->file, "%s # inline asm\n", asm_imm);
            break;
        }
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:   emit_comparison_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:    emit_comparison_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL: emit_comparison_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:    emit_comparison_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_LESS:          emit_comparison_instructions(context, instruction); break;
        case IR_INSTRUCTION_TYPE_GREATER:       emit_comparison_instructions(context, instruction); break;
        default: break;
    }
}



void x86_64_linux_emit_reg_size(struct codegen_context_t *context, enum register_size size) {
    switch (size) {
        case REGISTER_SIZE_8:  codegen_emit(context->file, "BYTE "); break;
        case REGISTER_SIZE_16: codegen_emit(context->file, "WORD "); break;
        case REGISTER_SIZE_32: codegen_emit(context->file, "DWORD "); break;
        case REGISTER_SIZE_64: codegen_emit(context->file, "QWORD "); break;
    }
}

void x86_64_linux_emit_reg(struct codegen_context_t *context, struct register_t *reg, enum register_size size, bool print_size) {
    const char *reg_str = context->build_target->get_register_name(context->build_target->registers, reg, size);
    codegen_emit(context->file, "%s ", reg_str);
}

void x86_64_linux_emit_operand(struct codegen_context_t *context, struct IR_Operand *op, enum register_size size, bool print_size) {
    switch (op->type) {
        case IR_OPERAND_TYPE_IMM: codegen_emit(context->file, "%s ", op->data.imm_value); break;
        case IR_OPERAND_TYPE_LABEL: codegen_emit(context->file, "%s ", op->data.mangled_label_name); break;
        case IR_OPERAND_TYPE_VREG: {
            if(REGISTER_SIZE_UNDEFINED == size) size = codegen_calc_register_size(op->type_info->size);
            x86_64_linux_emit_reg(context, op->data.vreg.reg, size, print_size);
            break;
        }case IR_OPERAND_TYPE_STACK_SLOT: {
            struct stack_slot_t *slot = op->data.slot.stack_slot;
            if(print_size) {
                x86_64_linux_emit_reg_size(context, codegen_calc_register_size(slot->type->size));
                codegen_emit(context->file, "PTR ");

                if(slot->is_argument) {
                    codegen_emit(context->file, "[rbp + %d] ", slot->stack_offset+16);
                }else {
                    codegen_emit(context->file, "[rbp - %d] ", slot->stack_offset);
                }
            }else {
                if(slot->is_argument) {
                    codegen_emit(context->file, "[rbp + %d] ", slot->stack_offset+16);
                }else {
                    codegen_emit(context->file, "[rbp - %d] ", slot->stack_offset);
                }
            }
            break;
        }
        case IR_OPERAND_TYPE_UNDEFINED: break;
    }
}
