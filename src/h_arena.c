#include "h_arena.h"

struct arena arena_create(void) {
    return (struct arena) {.first = NULL, .current = NULL};
}

static struct arena_chunk* arena_create_chunk(size_t size, size_t align) {
    size_t needed_cap = size + align;
    size_t cap = (needed_cap > H_ARENA_CHUNK_SIZE) ? needed_cap : H_ARENA_CHUNK_SIZE;

    struct arena_chunk *chunk = (struct arena_chunk*)calloc(1, sizeof(struct arena_chunk) + cap);
    if (!chunk) return NULL;

    chunk->next = NULL;
    chunk->capacity = cap;
    chunk->occupied = 0;
    return chunk;
}

void* arena_alloc_align(struct arena *arena, size_t size, size_t align) {
    if (size == 0 || arena == NULL) return NULL;

    if (!arena->current) {
        arena->first = arena_create_chunk(size, align);
        arena->current = arena->first;
        if (!arena->current) return NULL;
    }

    struct arena_chunk *current = arena->current;

    while (current) {
        size_t current_ptr = (size_t)(current->data + current->occupied);
        size_t aligned_ptr = arena_align_up(current_ptr, align);
        size_t offset = aligned_ptr - (size_t)current->data;

        if (offset + size <= current->capacity) {
            void *ptr = current->data + offset;
            current->occupied = offset + size;
            arena->current = current;
            memset(ptr, 0, size);
            return ptr;
        }

        if (current->next != NULL) {
            current = current->next;
        } else {
            struct arena_chunk *new_chunk = arena_create_chunk(size, align);
            if (!new_chunk) return NULL;

            current->next = new_chunk;
            current = new_chunk;
        }
    }

    return NULL;
}

void* arena_alloc(struct arena *arena, size_t size) {
    return arena_alloc_align(arena, size, _Alignof(max_align_t));
}

void arena_reset(struct arena *arena) {
    for (struct arena_chunk *current = arena->first; current != NULL; current = current->next) {
        current->occupied = 0;
    }
    arena->current = arena->first;
}

void arena_destroy(struct arena *arena) {
    struct arena_chunk *current = arena->first;
    while (current) {
        struct arena_chunk *next = current->next;
        free(current);
        current = next;
    }
    arena->first = NULL;
    arena->current = NULL;
}
char* arena_strdup(struct arena *arena, const char *str) {
    if (NULL == str || NULL == arena) return NULL;

    size_t len = strlen(str);
    char *dup = (char*)arena_alloc(arena, len + 1);
    if (NULL == dup) return NULL;

    memcpy(dup, str, len + 1);
    return dup;
}

char* arena_strndup(struct arena *arena, const char *str, size_t n) {
    if (NULL == str || NULL == arena) return NULL;

    size_t len = strlen(str);
    if (len > n) len = n;

    char *dup = (char*)arena_alloc(arena, len + 1);
    if (NULL == dup) return NULL;

    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}
