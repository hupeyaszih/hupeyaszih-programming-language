#ifndef CODEGEN_H
#define CODEGEN_H

#include "backend/x86_64/x86_64_linux.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>

enum register_bank {
    REGISTER_BANK_GPR,
    REGISTER_BANK_VECTOR
};

enum register_size {
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

    enum register_type type;
    enum register_size size;
    enum register_bank bank;

    int id;
    int arg_index;
    bool is_arg_reg;
    bool is_ret_reg;
};

struct register_list_t {
    struct register_t *registers;
    struct codegen_build_target_t *arch;

    int register_count;
};

struct codegen_build_target_t{
    struct register_list_t *registers;

    const char *(*get_register_name) (struct register_list_t *list, struct register_t *reg, enum register_size size);
    struct register_t *(*get_best_available_register) (struct register_list_t *list);
};

struct codegen_t {
    struct vector_t *build_targets; // struct codegen_build_target_t *
    struct codegen_build_target_t *current_build_target;
};

struct codegen_t *codegen_create_codegen();
void codegen_delete_codegen(struct codegen_t **codegen);

struct codegen_build_target_t *codegen_create_build_target();
void codegen_delete_build_target(struct codegen_build_target_t **target);

struct register_list_t *codegen_create_register_list(struct codegen_build_target_t *arch, int register_count);
void codegen_delete_register_list(struct register_list_t **list);

struct register_t *codegen_init_register(struct register_t *reg, int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg);
void codegen_delete_register(struct register_t *reg);

struct register_t *codegen_create_register_and_add_to_list(int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, struct register_list_t *list);


static inline void codegen_init_build_targets(struct codegen_t *codegen) {
    struct codegen_build_target_t *x86_64_linux = x86_64_linux_create_build_target();
    vector_add(codegen->build_targets, &x86_64_linux);

    // for(int i = 0;i < x86_64_linux->registers->register_count; ++i) {
    //     struct register_t *reg = x86_64_linux->registers->registers+i;
    //
    //     const char *name_8 = x86_64_linux->get_register_name(x86_64_linux->registers, reg, REGISTER_SIZE_8);
    //     const char *name_16 = x86_64_linux->get_register_name(x86_64_linux->registers, reg, REGISTER_SIZE_16);
    //     const char *name_32 = x86_64_linux->get_register_name(x86_64_linux->registers, reg, REGISTER_SIZE_32);
    //     const char *name_64 = x86_64_linux->get_register_name(x86_64_linux->registers, reg, REGISTER_SIZE_64);
    //     printf("%s %s %s %s\n", name_64, name_32, name_16, name_8);
    // }
}

static inline void codegen_register_add_name(struct register_t *reg ,char *name) {
    vector_add(reg->names, &name);
}

#endif
