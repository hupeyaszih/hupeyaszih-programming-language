#include "h_bitset.h"

struct bitset_t* bitset_create(size_t max_element_count) {
    struct bitset_t *bitset = calloc(1, sizeof(struct bitset_t));
    if (!bitset) {
        return NULL;
    }
    bitset->size = (max_element_count / 64) + 1;
    bitset->bits = calloc(bitset->size, sizeof(uint64_t));
    return bitset;
}

void bitset_free(struct bitset_t **bitset) {
    if (bitset == NULL || *bitset == NULL) {
        return;
    }
    free((*bitset)->bits);
    free((*bitset));
    *bitset = NULL;
}

void bitset_set(struct bitset_t *restrict bitset, int id) {
    bitset->bits[id / 64] |= (1ULL << (id % 64));
}

const bool bitset_test(const struct bitset_t *restrict bitset, int id) {
    return (bitset->bits[id / 64] & (1ULL << (id % 64))) != 0;
}

void bitset_or(struct bitset_t *restrict dest, struct bitset_t *src) {
    for (size_t i = 0; i < dest->size; i++) {
        dest->bits[i] |= src->bits[i];
    }
}


void bitset_and(struct bitset_t *restrict dest, struct bitset_t *src) {
    for (size_t i = 0; i < dest->size; i++) {
        dest->bits[i] &= src->bits[i];
    }
}


void bitset_copy(struct bitset_t *restrict dest, const struct bitset_t *src) {
    for (size_t i = 0; i < dest->size; i++) {
        dest->bits[i] = src->bits[i];
    }
}

const bool bitset_equals(const struct bitset_t *restrict a, const struct bitset_t *restrict b) {
    for (size_t i = 0; i < a->size; i++) {
        if (a->bits[i] != b->bits[i]) return false;
    }
    return true;
} 
