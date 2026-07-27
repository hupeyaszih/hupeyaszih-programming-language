#include "opt/opt.h"
#include "core/ir_gen.h"
#include "h_vector.h"

void opt_optimize_project(struct IR_Project *restrict project) {
    if(!project) return;

    struct opt_context_t opt_context;
    opt_context.register_allocator = register_allocator_create_register_allocator();

    for(int i = 0;i < project->modules->element_count; ++i) {
        struct IR_Module *module = *(struct IR_Module **) vector_get(project->modules, i);
        opt_optimize_module(&opt_context, module);
    }

}

void opt_optimize_module(struct opt_context_t *context, struct IR_Module *restrict module) {
    if(!module) return;

    for(int i = 0;i < module->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **) vector_get(module->functions, i);
        //
        register_allocator_run_allocator(context->register_allocator, function);
        //
    }

}
