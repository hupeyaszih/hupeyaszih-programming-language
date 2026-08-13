#ifndef H_ARENA_H
#define H_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define H_ARENA_CHUNK_SIZE (1024)

struct arena_chunk {
    struct arena_chunk *next;
    size_t capacity;
    size_t occupied;
    uint8_t data[];
};

struct arena {
    struct arena_chunk *first;
    struct arena_chunk *current;
};

struct arena arena_create(void);
void* arena_alloc_align(struct arena *arena, size_t size, size_t align);
void* arena_alloc(struct arena *arena, size_t size);
void arena_reset(struct arena *arena);
void arena_destroy(struct arena *arena);
char* arena_strdup(struct arena *arena, const char *str);
char* arena_strndup(struct arena *arena, const char *str, size_t n);
static inline size_t arena_align_up(size_t ptr, size_t align) {
    return (ptr + align - 1) & ~(align - 1);
}
#endif
