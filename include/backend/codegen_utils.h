#ifndef CODEGEN_UTILS_H
#define CODEGEN_UTILS_H

#include "backend/codegen.h"

void codegen_utils_emit_call_args(struct arena *arena, struct codegen_context_t *context, struct vector_t *arguments, struct vector_t *out_regs, int arg_count);

#endif
