#ifndef OPT_H
#define OPT_H

#include "core/ir_gen.h"
#include "opt/register_allocator.h"
#include <stdbool.h>

struct opt_context_t {
    struct register_allocator_t *register_allocator;
};



void opt_optimize_project(struct IR_Project *restrict project);
void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module);

void opt_run_cfg_analysis(struct IR_Function *function);
void opt_compute_use_def(struct IR_Function *function);
void opt_run_live_range_analysis(struct IR_Function *function);

#endif
