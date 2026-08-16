#ifndef H_VECTOR_H
#define H_VECTOR_H

#include "h_arena.h"
#include <stddef.h>

struct vector_t {
    struct arena *arena;
    void *data;
    size_t type_size;
    size_t element_count, capacity;
};

struct vector_t *vector_copy_vector(const struct vector_t *src); 
void vector_reset_vector(struct vector_t *vector);

struct vector_t *vector_create_vector(struct arena *arena, int capacity, size_t type_size); 

int vector_push(struct vector_t *restrict vector, void *data);
void *vector_pop(struct vector_t *restrict vector);

int vector_add(struct vector_t *restrict vector, void *data);
int vector_remove_at(struct vector_t *vector, size_t index);

void *vector_get(const struct vector_t *restrict vector, size_t index);


#endif
