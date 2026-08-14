#ifndef OPT_H
#define OPT_H

#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "opt/register_allocator.h"
#include <stdbool.h>

enum optimization_level {
    OPT_LEVEL_UNDEFINED = -1,
    OPT_LEVEL_O0 = 0,
    OPT_LEVEL_O1 = 1,
    OPT_LEVEL_O2 = 2,
    OPT_LEVEL_O3 = 3
};

struct opt_context_t {
    struct arena *temp_arena;
    struct register_allocator_t *register_allocator;
    struct codegen_t *codegen;
    enum optimization_level opt_level;
};



void opt_optimize_project(struct IR_Project *restrict project, struct codegen_t *codegen, enum optimization_level opt_level);
void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module);

void opt_run_cfg_analysis(struct IR_Function *function);
void opt_compute_use_def(struct arena *arena, struct IR_Function *function);
void opt_run_live_range_analysis(struct IR_Function *function, struct opt_context_t *context);

void opt_calculate_constants(struct IR_Function *function, struct opt_context_t *context);
bool opt_constant_folding(struct opt_context_t *context, struct IR_Function *function);
bool opt_copy_propagation(struct opt_context_t *context, struct IR_Function *function);
bool opt_dead_code_elimination(struct opt_context_t *context, struct IR_Function *function);
#endif
