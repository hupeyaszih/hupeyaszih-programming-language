#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "core/ir_gen.h"
struct register_allocator_t {

};

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function);

struct register_allocator_t *register_allocator_create_register_allocator();
void register_allocator_delete_register_allocator(struct register_allocator_t **register_allocator);

#endif
