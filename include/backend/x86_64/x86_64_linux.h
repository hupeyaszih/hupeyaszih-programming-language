#ifndef BACKEND_X86_64_LINUX_H
#define BACKEND_X86_64_LINUX_H


#include "h_arena.h"

struct codegen_context_t;

struct register_t;
struct IR_Function;
struct IR_Instruction;
struct IR_Operand;

enum register_size;
enum IR_Instruction_type;



struct codegen_build_target_t *x86_64_linux_create_build_target(struct arena *arena);
#endif
