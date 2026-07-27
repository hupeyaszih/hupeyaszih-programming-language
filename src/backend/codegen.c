#include "backend/codegen.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>

struct codegen_t *codegen_create_codegen() {
    struct codegen_t *codegen = calloc(1, sizeof(struct codegen_t));
    codegen->build_targets = vector_create_vector(2, sizeof(struct codegen_build_target_t *));
    codegen->current_build_target = NULL;

    return codegen;
}
void codegen_delete_codegen(struct codegen_t **codegen) {
    if(NULL == codegen || NULL == *codegen) return;
    for(int i = 0; i < (*codegen)->build_targets->element_count; ++i) {
        struct codegen_build_target_t **target = vector_get((*codegen)->build_targets, i);
        codegen_delete_build_target(target);
    }
    vector_free(&(*codegen)->build_targets);

    free(*codegen);

    *codegen = NULL;
}

struct codegen_build_target_t *codegen_create_build_target() {
    struct codegen_build_target_t *target = calloc(1, sizeof(struct codegen_build_target_t));
    return target;
}
void codegen_delete_build_target(struct codegen_build_target_t **target) {
    if(NULL == target || NULL == *target) return;
    codegen_delete_register_list(&(*target)->registers);
    free(*target);

    *target = NULL;
}

struct register_list_t *codegen_create_register_list(struct codegen_build_target_t *arch, int register_count) {
    struct register_list_t *list = calloc(1, sizeof(struct register_list_t));
    list->register_count = register_count;
    list->registers = calloc(register_count, sizeof(struct register_t));
    list->arch = arch;

    return list;
}
void codegen_delete_register_list(struct register_list_t **list) {
    if (!list || !*list) return;

    if ((*list)->registers) {
        for (int i = 0; i < (*list)->register_count; ++i) {
            codegen_delete_register(&(*list)->registers[i]); 
        }
        free((*list)->registers);
    }

    free(*list);
    *list = NULL;
}


struct register_t *codegen_init_register(struct register_t *reg, int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg) {
    reg->size = size;
    reg->id = id;
    reg->type = type;
    reg->bank = bank;
    reg->is_arg_reg = is_arg_reg;
    reg->is_ret_reg = is_ret_reg;
    reg->arg_index = arg_index;

    reg->names = vector_create_vector(4, sizeof(char *));

    return reg;
}
void codegen_delete_register(struct register_t *reg) {
    if (!reg) return;
    if (reg->names) {
        vector_free(&reg->names);
        reg->names = NULL;
    }
}

struct register_t *codegen_create_register_and_add_to_list(int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, struct register_list_t *list) {
    if (!list || !list->registers || id < 0 || id >= list->register_count) {
        return NULL;
    }
    codegen_init_register(&list->registers[id], id, size, type, bank, is_arg_reg, arg_index, is_ret_reg);
    return &list->registers[id];
}
