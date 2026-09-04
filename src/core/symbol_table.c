#include "core/symbol_table.h"
#include "core/parser.h"
#include "core/globals.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

struct symbol_table *symbol_table_create_symbol_table(struct arena *arena, struct symbol_table *restrict parent, int *global_scope_counter){
    struct symbol_table *table = arena_alloc(arena, sizeof(struct symbol_table));
    table->symbols = vector_create_vector(arena, 8, sizeof(struct symbol_t *));
    table->is_global_table = false;
    table->arena = arena;
    table->parent = parent;
    table->scope_level = (parent == NULL) ? 0 : parent->scope_level + 1;
    table->total_stack_size = 0;
    table->current_total_offset = 0;
    table->scope_id = (*global_scope_counter)++;
    if(0 == table->scope_level) {
        table->is_global_table = true;
    }
    return table;
}

struct symbol_t *symbol_table_define(struct symbol_table *restrict table, struct str_view name, struct type_info *restrict type, enum symbol_kind kind, int pointer_level, bool is_global){
    if(NULL == table){
        LOG_M_ERR("symbol_table_define - \"struct symbol_table *restrict table\" is null");
        return NULL;
    }
    if(NULL == type){
        C_LOG_ERR("Unknown type for \""SV_FMT"\"", SV_ARG(name));
        return NULL;
    }
    if (NULL == table->arena) {
        LOG_M_ERR("symbol_table_define - \"table->arena\" is null");
        return NULL;
    }
    if(str_view_is_empty(name)){
        LOG_M_ERR("symbol_table_define - \"char *restrict name\" is null");
        return NULL;
    }
    for (int i = 0; i < table->symbols->element_count; i++) {
        struct symbol_t *target_sym = *(struct symbol_t **) vector_get(table->symbols, i);
        if (str_view_eq(name, target_sym->name)) {
            C_LOG_ERR("'" SV_FMT "' already defined in this scope", SV_ARG(name));
            return NULL; 
        }
    }
    
    struct symbol_t *s = arena_alloc(table->arena, sizeof(struct symbol_t));
    s->name = name;
    s->is_global = is_global;
    s->mangled_name = NULL;
    s->type = type;
    s->kind = kind;
    s->pointer_level = pointer_level;
    if(type->category == TYPE_CATEGORY_ARRAY) {
        s->location_kind = LOCATION_STACK;
    }else if(!is_global) {
        s->location_kind = LOCATION_VREG;
    }else {
        s->location_kind = LOCATION_GLOBAL;
    }
    s->current_vreg = NULL;
    s->flags = 0;
    s->is_address_taken = false;
    s->stack_slot = NULL;

    vector_add(table->symbols, &s);

    return s;
}

struct symbol_t* symbol_table_look_up(const struct symbol_table *table, struct str_view name) {
    if(NULL == table){
        LOG_M_ERR("symbol_table_look_up - \"const struct symbol_table *table\" is null");
    }
    // if(NULL == name){
    //     LOG_M_ERR("symbol_table_look_up - \"char *name\" is null");
    // }
    while (table != NULL) {
        for (int i = 0; i < table->symbols->element_count; i++) {
            struct symbol_t *target_sym = *(struct symbol_t **) vector_get(table->symbols, i);
            if (str_view_eq(name, target_sym->name)) {
                return target_sym;
            }
        }
        table = table->parent; 
    }
    LOG_M_ERR("variable/function \"" SV_FMT "\" is not defined", SV_ARG(name));
    return NULL; 
}


struct type_table *type_table_create_type_table(struct arena *arena){
    struct type_table *table = arena_alloc(arena, sizeof(struct type_table));
    table->arena = arena;
    table->types = vector_create_vector(arena, 16, sizeof(struct type_info *));
    return table;
}

struct type_info *type_table_create_type_info_cstr(struct arena *arena, char *name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type, bool can_promote_to_memory_address) {
    size_t str_size = strlen(name);
    return type_table_create_type_info(arena, str_view_make(name, str_size), category, size, members, promotable_type, can_promote_to_memory_address);
}

struct type_info *type_table_create_type_info(struct arena *arena, struct str_view name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type, bool can_promote_to_memory_address) {
    static atomic_int id_counter = 0;
    struct type_info *info = arena_alloc(arena, sizeof(struct type_info));
    info->name = name;
    info->category = category;
    info->size = size;
    info->members = members;
    info->type_id = id_counter;
    info->promotable_type = promotable_type;
    info->points_to = NULL;
    info->pointer_level = 0;
    info->can_promote_to_memory_address = can_promote_to_memory_address;
    ++id_counter;
    return info;
}
struct type_info *type_table_get_type_info_cstr(const struct type_table *restrict table, char *name, int pointer_level){
    for(int i = 0; i < table->types->element_count; ++i){
        struct type_info *curr = *(struct type_info **) vector_get(table->types, i);
        if(curr->pointer_level == pointer_level && str_view_eq_cstr(curr->name, name)){
            return curr;
        }
    }
    return NULL;
}
struct type_info *type_table_get_type_info(const struct type_table *restrict table, const struct str_view name, int pointer_level){
    for(int i = 0; i < table->types->element_count; ++i){
        struct type_info *curr = *(struct type_info **) vector_get(table->types, i);
        if(curr->pointer_level == pointer_level && str_view_eq(name, curr->name)){
            return curr;
        }
    }
    return NULL;
}
struct type_info *type_table_get_or_create_pointer_type_info(struct type_table *restrict table, struct str_view name, int pointer_level){
    struct type_info *info = type_table_get_type_info(table, name, pointer_level);
    if(NULL != info) return info;

    for(int p = 1;p <= pointer_level; ++p) {
        struct type_info *curr = type_table_get_type_info(table, name, p);
        if(NULL != curr){
            info = curr;
            continue;
        } 
        info = type_table_create_type_info(table->arena, name, TYPE_CATEGORY_POINTER, table->pointers_size, NULL, NULL, false);
        info->pointer_level = p;
        info->points_to = type_table_get_type_info(table, name, p-1);
        info->points_to->pointer_type = info;
        type_table_insert(table, info);
    }

    return info;
}

struct type_info *type_table_dereference(struct type_table *type_table, struct type_info *type, int dereference_count) {
    while(dereference_count > 0) {
        if(type->category == TYPE_CATEGORY_ARRAY) {
            type = type_table_decay_array(type_table, type);
        }else {
            type = type_table_get_or_create_pointer_type_info(type_table, type->name, type->pointer_level-1);
        }
        --dereference_count;
    }
    return type;
}

struct type_info *type_table_create_array_info(struct type_table *table, struct type_info *element_info, size_t element_count) {
    size_t size = element_count * element_info->size;

    struct str_view name = str_view_fmt(table->arena, "[ptr:%d " SV_FMT ", %ld]", element_info->pointer_level, SV_ARG(element_info->name), element_count);

    struct type_info *decayed_type = type_table_get_or_create_pointer_type_info(table, element_info->name, element_info->pointer_level + 1);
    struct type_info *array_info = type_table_create_type_info(table->arena, name, TYPE_CATEGORY_ARRAY, size, NULL, decayed_type, false);

    array_info->array.element_info = element_info;
    array_info->array.element_count = element_count;

    type_table_insert(table, array_info);
    return array_info;
}
struct type_info *type_table_get_array_info(struct type_table *table, struct type_info *element_info, size_t element_count) {
    for(int i = 0; i < table->types->element_count; ++i) {
        struct type_info *info = *(struct type_info **) vector_get(table->types, i);
        if(!info) continue;
        if(info->category != TYPE_CATEGORY_ARRAY) continue;
        if(info->array.element_count != element_count) continue;
        if(info->array.element_info->type_id != element_info->type_id) continue;
        return info;
    }
    return NULL;
}
struct type_info *type_table_get_or_create_array_info(struct type_table *table, struct type_info *element_info, size_t element_count) {
    struct type_info *array_info = type_table_get_array_info(table, element_info, element_count);
    if(array_info) return array_info;
    return type_table_create_array_info(table, element_info, element_count);
}
struct type_info *type_table_decay_array(struct type_table *table, struct type_info *array_info) {
    if(NULL == array_info->promotable_type) {
        struct type_info *decayed_type = NULL;
        if(array_info->pointer_level == 0) {
            decayed_type = type_table_get_or_create_pointer_type_info(table, array_info->array.element_info->name, array_info->array.element_info->pointer_level + 1);
            array_info->promotable_type = decayed_type;
        }else {
            decayed_type = type_table_get_or_create_pointer_type_info(table, array_info->name, array_info->pointer_level - 1);
        }
        return decayed_type;
    }
    return array_info->promotable_type;
}

void type_table_insert(struct type_table *table, struct type_info *info) {
    vector_add(table->types, &info);
}


struct type_info *get_literals_type_info(struct type_table *type_table, struct type_info *target_info, enum parser_node_type literal_type) {
    switch (literal_type) {
        case PARSER_NODE_NUMBER:{
            struct type_info *type = type_table_get_type_info_cstr(type_table, "int8", 0);

            if(target_info && target_info->type_id != type->type_id && 1 == type_table_can_that_promote_to(type, target_info)) {
                return target_info;
            }
            return type;
        } case PARSER_NODE_STRING: {
            struct type_info *type = type_table_get_type_info_cstr(type_table, "char", 1);
            return type;
        }case PARSER_NODE_CHAR: {
            struct type_info *type = type_table_get_type_info_cstr(type_table, "char", 0);
            return type;
        }
    }

    return NULL;
}
