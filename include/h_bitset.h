#ifndef H_BITSET_H
#define H_BITSET_H

#include "h_arena.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct bitset_t {
    uint64_t *bits;
    size_t size; // (uint64_t *bits) count
    size_t max_element_count;
};


struct bitset_t* bitset_create(struct arena *arena, size_t max_element_count);

void bitset_set(struct bitset_t *bitset, int id);
void bitset_set_to_zero(struct bitset_t *restrict bitset, int id);
bool bitset_test(const struct bitset_t *restrict bitset, int id);

void bitset_or(struct bitset_t *restrict dest, struct bitset_t *src);
void bitset_and(struct bitset_t *restrict dest, struct bitset_t *src);
void bitset_copy(struct bitset_t *restrict dest, const struct bitset_t *src);
bool bitset_equals(const struct bitset_t *restrict a, const struct bitset_t *restrict b); 

void bitset_and_not(struct bitset_t *restrict dest, const struct bitset_t *restrict src);
void bitset_clear_all(struct bitset_t *bitset);

bool bitset_is_zero(struct bitset_t *src);
bool bitset_intersects(const struct bitset_t *restrict a, const struct bitset_t *restrict b);
#endif 
