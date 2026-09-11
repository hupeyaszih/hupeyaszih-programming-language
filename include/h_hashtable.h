#ifndef H_HASHTABLE_H
#define H_HASHTABLE_H

#include "h_arena.h"
#include <stddef.h>

enum hashtable_entry_state {HASH_TABLE_ENTRY_STATE_EMPTY = 0, HASH_TABLE_ENTRY_STATE_DELETED, HASH_TABLE_ENTRY_STATE_OCCUPIED};

struct hashtable_entry {
    void *data;
    enum hashtable_entry_state state;
    size_t key;
};

struct hashtable {
    size_t (*hash_function)(const void *key);

    struct hashtable_entry *entries;
    struct arena *arena;

    size_t element_size;

    size_t occupied_slot_count;
    size_t capacity;
};

struct hashtable *hashtable_create_table(struct arena *arena, size_t element_size, size_t init_capacity, size_t (*hash_function)(const void *key));

void hashtable_add(struct hashtable *table, void *data, size_t key);
void hashtable_delete(struct hashtable *table, size_t key);
void *hashtable_get(struct hashtable *table, size_t key);
void hashtable_clear(struct hashtable *table);

#endif
