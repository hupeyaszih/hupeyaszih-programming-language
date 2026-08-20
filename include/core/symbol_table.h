#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "h_string_view.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum parser_node_type;

enum symbol_kind {
    SYMBOL_KIND_VARIABLE,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_CONSTANT,
    SYMBOL_KIND_STRUCT // NOT SUPPORTED YET!
};

enum type_category {
    TYPE_CATEGORY_BASIC,   // int, float, bool
    TYPE_CATEGORY_STRUCT,  // NOT SUPPORTED YET!
    TYPE_CATEGORY_POINTER,
    TYPE_CATEGORY_ARRAY    // NOT SUPPORTED YET!
};

struct type_info {
    struct str_view name;                     // "int32", "float32" or "Point"
    struct symbol_table *members;   // For structs (not supported yet!)

    size_t size;                    // Byte
    enum type_category category;    // BASIC (int, float), STRUCT, ARRAY, POINTER

    struct type_info *promotable_type; // "int8" can promote to "int16"
    struct type_info *points_to; // "*int8" points to "int8" // for pointers
    struct type_info *pointer_type; // "int8"s pointer type is  "*int8" // for pointers
    int type_id;
    int pointer_level;
    bool can_promote_to_memory_address;
};

struct type_table{
    struct arena *arena;
    struct vector_t *types; // struct type_info *
    int pointers_size; // for 64-bit systems, this is 8
    struct type_info *pointer_to_int_type;
};


enum location_kind {
    LOCATION_VREG,
    LOCATION_STACK,
    LOCATION_GLOBAL
};


struct symbol_t{
    struct str_view name;
    char *mangled_name;
    struct type_info *type;

    union {
        struct IR_Operand *stack_slot;

        struct IR_Operand *current_vreg;

        struct IR_Operand *global;
    };

    union {
        struct{
            struct type_info *return_type;
            struct parser_node *parameters;
            struct IR_Function *ir_function;
        } function;
    };

    enum symbol_kind kind;
    enum location_kind location_kind;
    int pointer_level;
    struct bitset_t *flags;
    bool is_address_taken; // If it is true, this operand must be in the memory
    bool is_global;
};

struct symbol_table{  
    struct vector_t *symbols; // struct symbol_t *
    struct arena *arena;
    int scope_level;
    int total_stack_size;
    int current_total_offset;
    int scope_id;

    struct symbol_table *parent;
    struct vector_t *types;

    bool is_global_table;
};

struct symbol_table *symbol_table_create_symbol_table(struct arena *arena, struct symbol_table *restrict parent, int *global_scope_counter);

struct symbol_t *symbol_table_define(struct symbol_table *restrict table, struct str_view name, struct type_info *restrict type, enum symbol_kind kind, int pointer_level, bool is_global);

void symbol_table_assign(struct symbol_t *restrict symbol, int *current_stack_offset);
struct symbol_t* symbol_table_look_up(const struct symbol_table *table, struct str_view name);


struct type_table *type_table_create_type_table(struct arena *arena);

struct type_info *type_table_create_type_info(struct arena *arena, struct str_view name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type, bool can_promote_to_memory_address);
struct type_info *type_table_create_type_info_cstr(struct arena *arena, char *name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type, bool can_promote_to_memory_address);
struct type_info *type_table_get_type_info(const struct type_table *restrict table, const struct str_view name, int pointer_level);
struct type_info *type_table_get_type_info_cstr(const struct type_table *restrict table, char *name, int pointer_level);
struct type_info *type_table_get_or_create_pointer_type_info(struct type_table *restrict table, struct str_view name, int pointer_level);

struct type_info *get_literals_type_info(struct type_table *type_table, struct type_info *target_info, enum parser_node_type literal_type);

void type_table_insert(struct type_table *table, struct type_info *info);

static inline void type_table_init_builtins(struct type_table *table) {
    struct type_info *int64 = type_table_create_type_info_cstr(table->arena, "int64", TYPE_CATEGORY_BASIC, 8, NULL, NULL , true);
    struct type_info *int32 = type_table_create_type_info_cstr(table->arena, "int32", TYPE_CATEGORY_BASIC, 4, NULL, int64, true);
    struct type_info *int16 = type_table_create_type_info_cstr(table->arena, "int16", TYPE_CATEGORY_BASIC, 2, NULL, int32, true);
    struct type_info *int8  = type_table_create_type_info_cstr(table->arena, "int8", TYPE_CATEGORY_BASIC, 1, NULL, int16 , true);

    type_table_insert(table, int8);
    type_table_insert(table, int16);
    type_table_insert(table, int32);
    type_table_insert(table, int64);
    type_table_insert(table, type_table_create_type_info_cstr(table->arena, "bool", TYPE_CATEGORY_BASIC, 1, NULL, int8, false));

    type_table_insert(table, type_table_create_type_info_cstr(table->arena, "float64", TYPE_CATEGORY_BASIC, 8, NULL, NULL, false));
    type_table_insert(table, type_table_create_type_info_cstr(table->arena, "fn", TYPE_CATEGORY_BASIC, 8, NULL, NULL, false));

    table->pointers_size = 8;

    struct type_info *ch = type_table_create_type_info_cstr(table->arena, "char", TYPE_CATEGORY_BASIC, 1, NULL, int8, false);
    type_table_insert(table, ch);

    struct type_info *ch_p = type_table_get_or_create_pointer_type_info(table, str_view_from_cstr(table->arena, "char"), 1);

    struct type_info *str = type_table_create_type_info_cstr(table->arena, "string", TYPE_CATEGORY_POINTER, 8, NULL, ch_p, false);
    type_table_insert(table, str);

    table->pointer_to_int_type = int64;
}

static inline bool type_table_is_info_string(struct type_table *table, struct type_info *info) {
    if (!table || !info) return false;
    if(info->type_id == type_table_get_type_info_cstr(table, "string", 0)->type_id){
        return true;
    }
    return false;
}


static inline size_t type_table_size_padding(size_t type_size){
    return (type_size + 7) & ~7;
}

static inline int type_table_can_that_promote_to(struct type_info *type, struct type_info *target_type) { // if true, returns 1
    if (NULL == type || NULL == target_type) return 0;
    if (type->type_id == target_type->type_id) return 1;

    if(type->can_promote_to_memory_address && target_type->points_to) return 1;

    for(struct type_info *curr = type; curr != NULL; curr = curr->promotable_type) {
        if(curr->can_promote_to_memory_address && target_type->points_to) return 1;
        if(curr->type_id == target_type->type_id) {
            return 1;
        }
    }
    return 0;
}

static inline int type_table_calculate_pointer_level(struct type_info *type) {
    int pointer_level = 0;
    type = type->points_to;
    while(NULL != type) {
        ++pointer_level;
        type = type->points_to;
    }

    return pointer_level;
}

#endif
