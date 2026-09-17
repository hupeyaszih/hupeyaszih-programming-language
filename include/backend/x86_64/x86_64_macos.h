#ifndef BACKEND_X86_64_MACOS_H
#define BACKEND_X86_64_MACOS_H

#include "h_arena.h"
#include <stdbool.h>

struct codegen_context_t;

struct register_t;
struct IR_Function;
struct IR_Instruction;
struct IR_Operand;

enum register_size;
enum IR_Instruction_type;
struct str_view;



struct codegen_build_target_t *x86_64_macos_create_build_target(struct arena *arena);

void x86_64_macos_emit_jmp_main(struct codegen_context_t *context);
void x86_64_macos_emit_globals(struct codegen_context_t *context, bool jmp_to_main);
void x86_64_macos_emit_label(struct codegen_context_t *context, const struct str_view label, bool is_global);
struct str_view x86_64_macos_get_label_str(struct codegen_context_t *context, const struct str_view label, bool is_global);

#endif
