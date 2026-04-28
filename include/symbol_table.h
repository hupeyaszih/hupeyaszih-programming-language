#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>
#include <stdlib.h>

enum symbol_kind {
    SYMBOL_KIND_VARIABLE,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_CONSTANT,
    SYMBOL_KIND_STRUCT // NOT SUPPORTED YET!
};

enum type_category {
    TYPE_CATEGORY_BASIC,   // int, float, bool
    TYPE_CATEGORY_STRUCT,  // NOT SUPPORTED YET!
    TYPE_CATEGORY_POINTER, // NOT SUPPORTED YET!
    TYPE_CATEGORY_ARRAY    // NOT SUPPORTED YET!
};

struct type_info {
    char *name;                     // "int32", "float32" or "Point"
    enum type_category category;    // BASIC (int, float), STRUCT, ARRAY, POINTER
    size_t size;                    // Byte
    
    struct symbol_table *members;   // For structs (not supported yet!)
};

struct type_table{
    struct type_info **types;
    int count;
    int capacity;  
};

struct symbol_t{
    char *name;
    char *mangled_name;
    enum symbol_kind kind;
    struct type_info *type;

    int stack_offset;
};

struct symbol_table{  
    struct symbol_t *symbols;
    int symbol_count;
    int symbol_capacity;
    int scope_level;
    int total_stack_size;
    int current_total_offset;
    int scope_id;

    struct symbol_table *parent;
};

// struct symbol_table *symbol_table_create_symbol_table(struct symbol_table *restrict parent);
struct symbol_table *symbol_table_create_symbol_table(struct symbol_table *restrict parent, int *global_scope_counter);

struct symbol_t *symbol_table_define(struct symbol_table *restrict table, char *restrict name, struct type_info *restrict type, enum symbol_kind kind);

void symbol_table_assign(struct symbol_t *restrict symbol, int *current_stack_offset);
struct symbol_t* symbol_table_look_up(const struct symbol_table *table, const char *name);


struct type_table *type_table_create_type_table();
void type_table_delete_type_table(struct type_table **table);

struct type_info *type_table_create_type_info(char *name, enum type_category category, size_t size, struct symbol_table *members);
struct type_info *type_table_get_type_info(const struct type_table *restrict table, const char *restrict name);

void type_table_insert(struct type_table *table, struct type_info *info);

void symbol_table_delete_symbol_table(struct symbol_table **table);
void type_table_delete_type_info(struct type_info **info);

static inline void type_table_init_builtins(struct type_table *table) {
    type_table_insert(table, type_table_create_type_info("int32", TYPE_CATEGORY_BASIC, 4, NULL));
    type_table_insert(table, type_table_create_type_info("float64", TYPE_CATEGORY_BASIC, 8, NULL));
    type_table_insert(table, type_table_create_type_info("bool", TYPE_CATEGORY_BASIC, 1, NULL));
    type_table_insert(table, type_table_create_type_info("fn", TYPE_CATEGORY_BASIC, 8, NULL));
    type_table_insert(table, type_table_create_type_info("char", TYPE_CATEGORY_BASIC, 1, NULL));

    type_table_insert(table, type_table_create_type_info("ptr", TYPE_CATEGORY_POINTER, 8, NULL));
    // type_table_insert(table, type_table_create_type_info("string", TYPE_CATEGORY_POINTER, 8, NULL));
}

static inline size_t type_table_size_padding(size_t type_size){
    return (type_size + 7) & ~7;
}

#endif
