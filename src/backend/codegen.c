#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_vector.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct codegen_t *codegen_create_codegen(char *build_path) {
    struct codegen_t *codegen = calloc(1, sizeof(struct codegen_t));
    codegen->build_targets = vector_create_vector(2, sizeof(struct codegen_build_target_t *));
    codegen->current_build_target = NULL;
    codegen->build_path = build_path;

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


struct register_t *codegen_init_register(struct register_t *reg, int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, bool is_reserved) {
    reg->size = size;
    reg->id = id;
    reg->type = type;
    reg->bank = bank;
    reg->is_arg_reg = is_arg_reg;
    reg->is_ret_reg = is_ret_reg;
    reg->arg_index = arg_index;

    reg->names = vector_create_vector(4, sizeof(char *));

    reg->is_busy = false;
    reg->is_reserved = is_reserved;
    return reg;
}
void codegen_delete_register(struct register_t *reg) {
    if (!reg) return;
    if (reg->names) {
        vector_free(&reg->names);
        reg->names = NULL;
    }
}

struct register_t *codegen_create_register_and_add_to_list(int id, enum register_size size, enum register_type type, enum register_bank bank, bool is_arg_reg, int arg_index, bool is_ret_reg, bool is_reserved, struct register_list_t *list) {
    if (!list || !list->registers || id < 0 || id >= list->register_count) {
        return NULL;
    }
    codegen_init_register(&list->registers[id], id, size, type, bank, is_arg_reg, arg_index, is_ret_reg, is_reserved);
    return &list->registers[id];
}


// build
void codegen_build_project(struct codegen_t *codegen, struct IR_Project *project) {
    for(int i = 0;i < project->modules->element_count; ++i) {
        struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
        codegen_build_module(codegen, module);
    }
}

void codegen_build_module(struct codegen_t *codegen, struct IR_Module *module) {
    struct codegen_build_target_t *target = codegen->current_build_target;
    struct codegen_context_t context;
    context.codegen = codegen;
    context.build_target = codegen->current_build_target;
    context.current_function = NULL;
    context.main_function = module->parent_project->main_function;
    context.stack_slot_names = vector_create_vector(4, sizeof(char *));

    char filepath[1024];
    size_t len = strlen(codegen->build_path);
    if (len > 0 && codegen->build_path[len - 1] == '/') {
        snprintf(filepath, sizeof(filepath), "%s%s.s", codegen->build_path, module->name);
    } else {
        snprintf(filepath, sizeof(filepath), "%s/%s.s", codegen->build_path, module->name);
    }

    context.file = fopen(filepath, "w");
    if (!context.file) {
        fprintf(stderr, "[Codegen Error] Unable to open output file: %s\n", filepath);
        return;
    }

    bool jmp_to_main = module->parent_project->main_module == module;
    target->emit_globals(&context, jmp_to_main);
    for(int i = 0;i < module->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **) vector_get(module->functions, i);
        codegen_build_function(&context, function);
    }

    fclose(context.file);
    context.file = NULL;

    for(int i = 0;i < context.stack_slot_names->element_count; ++i) {
        char *name = *(char **) vector_get(context.stack_slot_names, i);
        free(name);
    }

    vector_free(&context.stack_slot_names);
}

void codegen_build_function(struct codegen_context_t *context, struct IR_Function *function) {
    context->current_function = function;
    struct codegen_build_target_t *target = context->build_target;

    target->emit_function_prologue(context, function);


    struct IR_Block *block = function->head_block;
    while(NULL != block) {
        target->emit_label(context, block->mangled_name, false);
        struct IR_Instruction *instruction = block->head_instruction;
        while(NULL != instruction) {
            target->emit_instruction(context, instruction);
            instruction = instruction->next;
        }

        block = block->next;
    }
    target->emit_function_epilogue(context, function);
}

enum register_size codegen_get_register_size_from_operand(struct IR_Operand *operand) {
    switch(operand->type) {
        case IR_OPERAND_TYPE_VREG: {
            return codegen_calc_register_size(operand->type_info->size);
        }case IR_OPERAND_TYPE_STACK_SLOT: {
            return codegen_calc_register_size(operand->data.slot.stack_slot->type->size);
        } case IR_OPERAND_TYPE_IMM: {
            return codegen_calc_register_size(operand->type_info->size);
        }
        default: return REGISTER_SIZE_UNDEFINED;
    }
    return REGISTER_SIZE_UNDEFINED;
}
int codegen_compare_register_sizes(enum register_size size_1, enum register_size size_2) {
    if(size_1 == size_2) return 0;
    if(size_1 < size_2) return 1;
    return -1;
}

void codegen_emit(FILE *file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

bool codegen_is_register_clobbered_for_vreg(struct register_t *reg, struct IR_Operand *vreg, struct vector_t *clobber_list) {
    if (!reg || !vreg || !clobber_list) return false;

    if(IR_OPERAND_TYPE_VREG != vreg->type) return false;

    int vreg_start = vreg->data.vreg.live_interval.start;
    int vreg_end   = vreg->data.vreg.live_interval.end;

    for (size_t i = 0; i < clobber_list->element_count; ++i) {
        struct clobber_t *clobber = vector_get(clobber_list, i);
        
        if (clobber->reg == reg) {
            if (clobber->time >= vreg_start && clobber->time <= vreg_end) {
                return true;
            }
        }
    }

    return false;
}
