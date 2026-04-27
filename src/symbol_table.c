#include "symbol_table.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct symbol_table *symbol_table_create_symbol_table(struct symbol_table *restrict parent, int *global_scope_counter){
    struct symbol_table *table = calloc(1, sizeof(struct symbol_table));
    table->symbol_capacity = 16;
    table->symbols = calloc(table->symbol_capacity, sizeof(struct symbol_t));
    table->symbol_count = 0;
    table->parent = parent;
    table->scope_level = (parent == NULL) ? 0 : parent->scope_level + 1;
    table->total_stack_size = 0;
    table->current_total_offset = 0;
    table->scope_id = (*global_scope_counter)++;
    return table;
}

struct symbol_t *symbol_table_define(struct symbol_table *restrict table, char *restrict name, struct type_info *restrict type, enum symbol_kind kind){
    if(NULL == table){
        LOG_M_ERR("symbol_table_define - \"struct symbol_table *restrict table\" is null");
        return NULL;
    }
    if(NULL == type){
        C_LOG_ERR("Unknown type for \"%s\"", name);
        return NULL;
    }
    if(NULL == name){
        LOG_M_ERR("symbol_table_define - \"char *restrict name\" is null");
        return NULL;
    }
    if (table->symbol_count >= table->symbol_capacity) {
        struct symbol_t *tmp = realloc(table->symbols, sizeof(struct symbol_t) * table->symbol_capacity*2);
        if(NULL == tmp) {
            LOG_M_ERR("symbol_table_define - \"struct symbol_t *tmp\" is null");
            free(tmp);
            return NULL;
        }
        table->symbol_capacity *= 2;
        table->symbols = tmp;
    }
    for (int i = 0; i < table->symbol_count; i++) {
        if (0 == strcmp(table->symbols[i].name, name)) {
            C_LOG_ERR("'%s' already defined in this scope", name);
            return NULL; 
        }
    }
    
    struct symbol_t *s = &table->symbols[table->symbol_count++];
    s->name = strdup(name);
    s->mangled_name = strdup(name);
    s->type = type;
    s->kind = kind;

    return s;
}

struct symbol_t* symbol_table_look_up(const struct symbol_table *table, const char *name) {
    if(NULL == table){
        LOG_M_ERR("symbol_table_look_up - \"const struct symbol_table *table\" is null");
    }
    if(NULL == name){
        LOG_M_ERR("symbol_table_look_up - \"char *name\" is null");
    }
    while (table != NULL) {
        for (int i = 0; i < table->symbol_count; i++) {
            if (strcmp(table->symbols[i].name, name) == 0) {
                return &table->symbols[i];
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

struct type_info *type_table_create_type_info(char *name, enum type_category category, size_t size, struct symbol_table *members){
    struct type_info *info = malloc(sizeof(struct type_info));
    info->name = strdup(name);
    info->category = category;
    info->size = size;
    info->members = members;
    return info;
}
struct type_info *type_table_get_type_info(const struct type_table *restrict table, const char *restrict name){
    for(int i = 0; i < table->count; ++i){
        if(0 == strcmp(table->types[i]->name, name)){
            return table->types[i];
        }
    }
    return NULL;
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
    for (int i = 0; i < (*table)->symbol_count; i++) {
        free((*table)->symbols[i].name);
        free((*table)->symbols[i].mangled_name);
    }
    free((*table)->symbols);
    free(*table);
    *table = NULL;
}
void type_table_delete_type_info(struct type_info **info){
    free((*info)->name);
    if(NULL != (*info)->members)symbol_table_delete_symbol_table(&(*info)->members);
    free((*info));

    *info = NULL;
}
