#include "h_hashtable.h"
#include "h_arena.h"
#include "math/h_math.h"
#include "stdbool.h"

// static inline size_t calc_index(struct hashtable *table, size_t key) {
//     size_t hash = table->hash_function(&key);
//     return -1 == hash ? -1 : hash % table->capacity;
// }
static inline size_t calc_index(struct hashtable *table, size_t key) {
    if (key == (size_t)-1) return -1;
    return key % table->capacity;
}

static inline struct hashtable_entry *get_entry(struct hashtable *table, size_t index) {return table->entries+index;}

struct hashtable *hashtable_create_table(struct arena *arena, size_t element_size, size_t init_capacity, size_t (*hash_function)(const void *key)) {
    struct hashtable *table = arena_alloc(arena, sizeof(struct hashtable));
    table->capacity = init_capacity;
    table->occupied_slot_count = 0;
    table->entries = arena_alloc(arena, sizeof(struct hashtable_entry) * init_capacity);
    table->hash_function = hash_function;

    table->arena = arena;
    table->element_size = element_size;
    return table;
}

void hashtable_clear(struct hashtable *table) {
    memset(table->entries, 0, sizeof(struct hashtable_entry) * table->capacity);
    table->occupied_slot_count = 0;
}

static inline bool is_table_fully_loaded(size_t capacity, size_t occupied_slot_count) {
    if (capacity == 0) return true;
    return (occupied_slot_count * 10) > (capacity * 7);
}

static inline bool hashtable_enlarge(struct hashtable *table) {
    size_t new_capacity = hmath_find_next_prime(table->capacity);
    struct hashtable_entry *temp_entries = arena_alloc(table->arena, sizeof(struct hashtable_entry) * new_capacity);
    if(!temp_entries) return false;

    struct hashtable_entry *old_entries = table->entries;
    size_t old_capacity = table->capacity;
    table->entries = temp_entries;
    table->capacity = new_capacity;

    table->occupied_slot_count = 0;
    for(int i = 0;i < old_capacity; ++i) {
        struct hashtable_entry *old_entry = old_entries + i;
        if(HASH_TABLE_ENTRY_STATE_OCCUPIED != old_entry->state) continue;
        hashtable_add(table, old_entry->data, old_entry->key);
    }

    return true;
}

void hashtable_add(struct hashtable *table, void *data, size_t key) {
    if(is_table_fully_loaded(table->capacity, table->occupied_slot_count)) {
        if(!hashtable_enlarge(table)) return;
    }

    size_t index = calc_index(table, key);
    size_t original_index = index;
    struct hashtable_entry *entry = get_entry(table, index);
    
    size_t first_available_index = (size_t)-1;
    bool has_available = false;

    while (entry->state != HASH_TABLE_ENTRY_STATE_EMPTY) {
        if (HASH_TABLE_ENTRY_STATE_OCCUPIED == entry->state && entry->key == key) {
            entry->data = data;
            return;
        }
        if (HASH_TABLE_ENTRY_STATE_DELETED == entry->state && !has_available) {
            first_available_index = index;
            has_available = true;
        }
        
        index = (index + 1) % table->capacity;
        if (index == original_index) break;
        entry = get_entry(table, index);
    }

    if (has_available) {
        index = first_available_index;
        entry = get_entry(table, index);
    }

    if (HASH_TABLE_ENTRY_STATE_OCCUPIED != entry->state) {
        ++table->occupied_slot_count;
    }

    entry->data = data;
    entry->state = HASH_TABLE_ENTRY_STATE_OCCUPIED;
    entry->key = key;
}

void *hashtable_get(struct hashtable *table, size_t key) {
    size_t index = calc_index(table, key);
    size_t original_index = index;
    struct hashtable_entry *entry = get_entry(table, index);
    
    while (HASH_TABLE_ENTRY_STATE_EMPTY != entry->state) {
        if (HASH_TABLE_ENTRY_STATE_OCCUPIED == entry->state && entry->key == key) {
            return entry->data;
        }
        index = (index + 1) % table->capacity;
        if (index == original_index) break;
        entry = get_entry(table, index);
    }
    return NULL;
}

void hashtable_delete(struct hashtable *table, size_t key) {
    size_t index = calc_index(table, key);
    size_t original_index = index;
    struct hashtable_entry *entry = get_entry(table, index);
    
    while (HASH_TABLE_ENTRY_STATE_EMPTY != entry->state) {
        if (HASH_TABLE_ENTRY_STATE_OCCUPIED == entry->state && entry->key == key) {
            entry->data = NULL;
            entry->key = -1;
            entry->state = HASH_TABLE_ENTRY_STATE_DELETED;
            --table->occupied_slot_count;
            return;
        }
        index = (index + 1) % table->capacity;
        if (index == original_index) break;
        entry = get_entry(table, index);
    }
}
