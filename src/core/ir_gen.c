#include "core/ir_gen.h"

struct IR_Module *IR_create_IR_Module();
void IR_delete_IR_Module(struct IR_Module **module);

struct IR_Function *IR_create_IR_Function(char *name, char *mangled_name);
void IR_delete_IR_Function(struct IR_Function **function);

struct IR_Block *IR_create_IR_Block(char *name, char *mangled_name);
void IR_delete_IR_Block(struct IR_Block **block);

struct IR_Instruction *IR_create_IR_Instruction(enum IR_Instruction_type type, int id);
void IR_delete_IR_Instruction(struct IR_Instruction **instruction);

struct IR_Operand *IR_create_IR_Operand(enum IR_Operand_type type);
void IR_delete_IR_Operand(struct IR_Operand **operand);
