#include "backend/x86_64/x86_64_linux.h"
#include "backend/codegen.h"
#include <stdio.h>
#include <sys/types.h>

struct codegen_build_target_t *x86_64_linux_create_build_target() {
    struct codegen_build_target_t *target = codegen_create_build_target();

    target->registers = x86_64_linux_create_register_list(target);
    target->get_register_name = &x86_64_linux_get_register_name;
    target->get_best_available_register = &x86_64_linux_get_best_available_register;
    return target;
}

struct register_list_t *x86_64_linux_create_register_list(struct codegen_build_target_t *arch) {
    const int register_count = 16; // general purpose register count
    struct register_list_t *list = codegen_create_register_list(arch, register_count);
    arch->registers = list;

    struct register_t *rax = codegen_create_register_and_add_to_list(0 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, true, list);
    struct register_t *rdi = codegen_create_register_and_add_to_list(1 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 0, false, list);
    struct register_t *rsi = codegen_create_register_and_add_to_list(2 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 1, false, list);
    struct register_t *rdx = codegen_create_register_and_add_to_list(3 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 2, false, list);
    struct register_t *rcx = codegen_create_register_and_add_to_list(4 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 3, false, list);
    struct register_t *r8  = codegen_create_register_and_add_to_list(5 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 4, false, list);
    struct register_t *r9  = codegen_create_register_and_add_to_list(6 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, true, 5, false, list);
    struct register_t *r10 = codegen_create_register_and_add_to_list(7 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *r11 = codegen_create_register_and_add_to_list(8 , REGISTER_SIZE_64, REGISTER_TYPE_CALLER_SAVED, REGISTER_BANK_GPR, false, -1, false, list);

    struct register_t *rsp = codegen_create_register_and_add_to_list(9 , REGISTER_SIZE_64, REGISTER_TYPE_RESERVED    , REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *rbp = codegen_create_register_and_add_to_list(10, REGISTER_SIZE_64, REGISTER_TYPE_RESERVED    , REGISTER_BANK_GPR, false, -1, false, list);

    struct register_t *rbx = codegen_create_register_and_add_to_list(11, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *r12 = codegen_create_register_and_add_to_list(12, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *r13 = codegen_create_register_and_add_to_list(13, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *r14 = codegen_create_register_and_add_to_list(14, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, list);
    struct register_t *r15 = codegen_create_register_and_add_to_list(15, REGISTER_SIZE_64, REGISTER_TYPE_CALLEE_SAVED, REGISTER_BANK_GPR, false, -1, false, list);

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

const char *x86_64_linux_get_register_name(struct register_list_t *list, struct register_t *reg, enum register_size size) {
    return *(char **) vector_get(reg->names, size);
}

struct register_t *x86_64_linux_get_best_available_register(struct register_list_t *list) {

    return NULL;
}
