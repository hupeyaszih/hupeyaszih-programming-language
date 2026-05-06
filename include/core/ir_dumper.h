#ifndef IR_DUMPER_H
#define IR_DUMPER_H

#include "ir_gen.h"

void IR_dump_operand(struct IR_Operand *op);
void IR_dump_instruction(struct IR_Instruction *inst);
void IR_dump_block(struct IR_Block *block);
void IR_dump_function(struct IR_Function *func);
void IR_dump_module(struct IR_Module *module);

#endif
