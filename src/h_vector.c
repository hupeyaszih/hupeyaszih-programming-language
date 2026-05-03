#include "h_vector.h"
#include <stdlib.h>
#include <string.h>


struct vector_t *vector_create_vector(int capacity, size_t type_size) {
    if(capacity <= 0 || type_size <= 0) {
        return NULL;
    }
    struct vector_t *vector = calloc(1, sizeof(struct vector_t));
    vector->type_size = type_size;
    vector->capacity = capacity;
    vector->element_count = 0;
    vector->data = calloc(vector->capacity, vector->type_size);

    return vector;
}

int vector_add(struct vector_t *restrict vector, void *data){
    if(vector->element_count+1 >= vector->capacity) {
        vector->capacity *= 2;
        void *tmp = realloc(vector->data, vector->capacity * vector->type_size);
        if(NULL == tmp) {
            return 1;
        }
        size_t old_size = (vector->capacity / 2) * vector->type_size;
        size_t new_size = vector->capacity * vector->type_size;
        vector->data = tmp;
        memset((char*)vector->data + old_size, 0, new_size - old_size);
    }

    void *target = (char*) vector->data + (vector->type_size * vector->element_count);
    memcpy(target, data, vector->type_size);
    vector->element_count += 1;
    return 0;
}

void *vector_get(const struct vector_t *restrict vector, size_t index) {
    if(index >= vector->element_count) {
        return NULL;
    }
    
    void *element = (char*) vector->data + (vector->type_size * index);
    return element;
}

int vector_free(struct vector_t **restrict vector) {
    if(NULL == vector || NULL == *vector) return 1;

    if((*vector)->data) free((*vector)->data);
    free((*vector));
    *vector = NULL;
    return 0;
}
