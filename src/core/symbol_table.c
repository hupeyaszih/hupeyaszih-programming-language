#include "core/symbol_table.h"
#include "core/parser.h"
#include "core/globals.h"
#include "h_string_view.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

struct symbol_table *symbol_table_create_symbol_table(struct symbol_table *restrict parent, int *global_scope_counter){
    struct symbol_table *table = calloc(1, sizeof(struct symbol_table));
    table->symbols = vector_create_vector(8, sizeof(struct symbol_t *));
    table->parent = parent;
    table->scope_level = (parent == NULL) ? 0 : parent->scope_level + 1;
    table->total_stack_size = 0;
    table->current_total_offset = 0;
    table->scope_id = (*global_scope_counter)++;
    return table;
}

struct symbol_t *symbol_table_define(struct symbol_table *restrict table, struct str_view name, struct type_info *restrict type, enum symbol_kind kind, int pointer_level){
    if(NULL == table){
        LOG_M_ERR("symbol_table_define - \"struct symbol_table *restrict table\" is null");
        return NULL;
    }
    if(NULL == type){
        C_LOG_ERR("Unknown type for \""SV_FMT"\"", SV_ARG(name));
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
    
    struct symbol_t *s = calloc(1, sizeof(struct symbol_t));
    s->name = name;
    s->mangled_name = NULL;
    s->type = type;
    s->kind = kind;
    s->pointer_level = pointer_level;
    s->location_kind = LOCATION_VREG;
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
    LOG_M_ERR("variable/function \"%s\" is not defined", name);
    return NULL; 
}


struct type_table *type_table_create_type_table(){
    struct type_table *table = malloc(sizeof(struct type_table));
    table->capacity = 16;
    table->count = 0;
    table->types = malloc(sizeof(struct type_info *) * table->capacity);
    return table;
}
void type_table_delete_type_table(struct type_table **table){
    if(NULL == table || NULL == *table) {
        LOG_M_ERR("type_table_delete_type_table - \"struct type_table **table\" or \"*table\" is null");
        return;
    }
    for(int i = 0; i < (*table)->count; ++i){
        type_table_delete_type_info(&(*table)->types[i]);
    }
    free((*table)->types);
    (*table)->types = NULL;
    free((*table));
    (*table) = NULL;
}


struct type_info *type_table_create_type_info_cstr(char *name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type) {
    size_t str_size = strlen(name);
    return type_table_create_type_info(str_view_make(name, str_size), category, size, members, promotable_type);
}

struct type_info *type_table_create_type_info(struct str_view name, enum type_category category, size_t size, struct symbol_table *members, struct type_info *promotable_type) {
    static atomic_int id_counter = 0;
    struct type_info *info = malloc(sizeof(struct type_info));
    info->name = name;
    info->category = category;
    info->size = size;
    info->members = members;
    info->type_id = id_counter;
    info->promotable_type = promotable_type;
    info->points_to = NULL;
    info->pointer_level = 0;
    ++id_counter;
    return info;
}
struct type_info *type_table_get_type_info_cstr(const struct type_table *restrict table, char *name, int pointer_level){
    for(int i = 0; i < table->count; ++i){
        if(table->types[i]->pointer_level == pointer_level && str_view_eq_cstr(table->types[i]->name, name)){
            return table->types[i];
        }
    }
    return NULL;
}
struct type_info *type_table_get_type_info(const struct type_table *restrict table, const struct str_view name, int pointer_level){
    for(int i = 0; i < table->count; ++i){
        if(table->types[i]->pointer_level == pointer_level && str_view_eq(name, table->types[i]->name)){
            return table->types[i];
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
        info = type_table_create_type_info(name, TYPE_CATEGORY_POINTER, table->pointers_size, NULL, NULL);
        info->pointer_level = p;
        info->points_to = type_table_get_type_info(table, name, p-1);
        info->points_to->pointer_type = info;
        type_table_insert(table, info);
    }

    return info;
}

void type_table_insert(struct type_table *table, struct type_info *info) {
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->types = realloc(table->types, sizeof(struct type_info *) * table->capacity);
    }
    table->types[table->count++] = info;
}

void symbol_table_delete_symbol_table(struct symbol_table **table){
    if(NULL == table || NULL == *table) {
        LOG_M_ERR("symbol_table_delete_symbol_table - \"struct symbol_table **table\" or \"*table\" is null");
        return;
    }
    for (int i = 0; i < (*table)->symbols->element_count; i++) {
        struct symbol_t *sym = *(struct symbol_t **) vector_get((*table)->symbols, i);
        free(sym->mangled_name);
        free(sym);
    }
    vector_free(&(*table)->symbols);
    free(*table);
    *table = NULL;
}
void type_table_delete_type_info(struct type_info **info){
    if(NULL != (*info)->members)symbol_table_delete_symbol_table(&(*info)->members);
    free((*info));

    *info = NULL;
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
            struct type_info *type = type_table_get_type_info_cstr(type_table, "string", 1);
            return type;
        }
    }

    return NULL;
}
