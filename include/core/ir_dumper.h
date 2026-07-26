#ifndef IR_DUMPER_H
#define IR_DUMPER_H

#include "core/ir_gen.h"

void IR_dump_module(const struct IR_Module *restrict module);

static void IR_dump_function(const struct IR_Function *function);
static void IR_dump_block(const struct IR_Block *block);
static void IR_dump_instruction(const struct IR_Instruction *instruction);

#endif
