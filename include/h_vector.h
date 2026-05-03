#ifndef H_VECTOR_H
#define H_VECTOR_H

#include <stddef.h>

struct vector_t {
    void *data;
    size_t type_size;
    size_t element_count, capacity;
};

struct vector_t *vector_create_vector(int capacity, size_t type_size); 
int vector_free(struct vector_t **restrict vector);

int vector_add(struct vector_t *restrict vector, void *data);
void *vector_get(const struct vector_t *restrict vector, size_t index);

#endif
