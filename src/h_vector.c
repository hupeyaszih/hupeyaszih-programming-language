#include "h_vector.h"
#include "h_arena.h"
#include <stdlib.h>
#include <string.h>


struct vector_t *vector_create_vector(struct arena *arena, int capacity, size_t type_size) {
    if(capacity <= 0 || type_size <= 0) {
        return NULL;
    }
    struct vector_t *vector = arena_alloc(arena, sizeof(struct vector_t));
    vector->arena = arena;
    vector->type_size = type_size;
    vector->capacity = capacity;
    vector->element_count = 0;
    vector->data = arena_alloc(arena,vector->capacity * vector->type_size);

    return vector;
}

void vector_reset_vector(struct vector_t *vector) {
    vector->element_count = 0;
}

int vector_push(struct vector_t *restrict vector, void *data) {
    return vector_add(vector, data);
}
void *vector_pop(struct vector_t *restrict vector) {
    void *r_val = vector_get(vector, vector->element_count-1);
    --vector->element_count;
    return r_val;
}

int vector_add(struct vector_t *restrict vector, void *data) {
    if (NULL == vector || NULL == data) {
        return 1;
    }

    if (vector->element_count + 1 >= vector->capacity) {
        size_t old_cap = vector->capacity;
        size_t new_cap = (old_cap == 0) ? 8 : old_cap * 2;

        size_t old_bytes = old_cap * vector->type_size;
        size_t new_bytes = new_cap * vector->type_size;

        void *new_data = NULL;

        if (vector->arena != NULL) {
            new_data = arena_alloc(vector->arena, new_bytes);
            if (NULL == new_data) return 1;

            if (vector->data && old_bytes > 0) {
                memcpy(new_data, vector->data, old_bytes);
            }
        } 
        else {
            new_data = realloc(vector->data, new_bytes);
            if (NULL == new_data) return 1;

            memset((char*)new_data + old_bytes, 0, new_bytes - old_bytes);
        }

        vector->data = new_data;
        vector->capacity = new_cap;
    }

    void *target = (char*) vector->data + (vector->type_size * vector->element_count);
    memcpy(target, data, vector->type_size);
    vector->element_count += 1;

    return 0;
}

int vector_remove_at(struct vector_t *vector, size_t index) {
    if (NULL == vector || index >= vector->element_count) {
        return 1;
    }

    if (index < vector->element_count - 1) {
        void *target = (char *)vector->data + (index * vector->type_size);
        void *source = (char *)vector->data + ((index + 1) * vector->type_size);
        size_t bytes_to_move = (vector->element_count - index - 1) * vector->type_size;

        memmove(target, source, bytes_to_move);
    }

    vector->element_count -= 1;

    void *last_slot = (char *)vector->data + (vector->element_count * vector->type_size);
    memset(last_slot, 0, vector->type_size);

    return 0;
}

void *vector_get(const struct vector_t *restrict vector, size_t index) {
    if(index >= vector->element_count) {
        return NULL;
    }
    
    void *element = (char*) vector->data + (vector->type_size * index);
    return element;
}
