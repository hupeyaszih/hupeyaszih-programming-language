#ifndef H_BITSET_H
#define H_BITSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct bitset_t {
    uint64_t *bits;
    size_t size; // (uint64_t *bits) count
};


struct bitset_t* bitset_create(size_t max_element_count);
void bitset_free(struct bitset_t **bitset);

void bitset_set(struct bitset_t *bitset, int id);
const bool bitset_test(const struct bitset_t *restrict bitset, int id);

void bitset_or(struct bitset_t *restrict dest, struct bitset_t *src);
void bitset_and(struct bitset_t *restrict dest, struct bitset_t *src);
void bitset_copy(struct bitset_t *restrict dest, const struct bitset_t *src);
const bool bitset_equals(const struct bitset_t *restrict a, const struct bitset_t *restrict b); 

#endif 
