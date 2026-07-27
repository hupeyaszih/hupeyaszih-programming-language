#include "opt/register_allocator.h"
#include <stdlib.h>

struct register_allocator_t *register_allocator_create_register_allocator() {
    struct register_allocator_t *allocator = calloc(1, sizeof(struct register_allocator_t));
    return allocator;
}

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function) {
    if(!function || !allocator) return;
}
