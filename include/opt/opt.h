#ifndef OPT_H
#define OPT_H

#include "opt/register_allocator.h"
#include "core/ir_gen.h"

struct opt_context_t {
    struct register_allocator_t *register_allocator;
};

void opt_optimize_project(struct IR_Project *restrict project);
void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module);

#endif
