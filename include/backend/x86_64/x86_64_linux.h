#ifndef BACKEND_X86_64_LINUX_H
#define BACKEND_X86_64_LINUX_H


#define X86_64_RAX (0)
#define X86_64_RDI (1)
#define X86_64_RSI (2)
#define X86_64_RDX (3)
#define X86_64_RCX (4)
#define X86_64_R8  (5)
#define X86_64_R9  (6)
#define X86_64_R10 (7)
#define X86_64_R11 (8)

#define X86_64_RSP (9)
#define X86_64_RBP (10)

#define X86_64_RBX (11)
#define X86_64_R12 (12)
#define X86_64_R13 (13)
#define X86_64_R14 (14)
#define X86_64_R15 (15)
#define X86_64_REGISTER_COUNT (16)


#include <stdbool.h>

struct register_t;
struct IR_Instruction;
struct IR_Operand;

enum register_size;
enum IR_Instruction_type;



struct codegen_build_target_t *x86_64_linux_create_build_target();

struct register_list_t *x86_64_linux_create_register_list(struct codegen_build_target_t *arch);

const char *x86_64_linux_get_register_name(struct register_list_t *list, struct register_t *reg, enum register_size size);
struct register_t *x86_64_linux_get_best_available_register(struct register_list_t *list, struct register_t *preferred_register);

struct register_t *x86_64_linux_get_fixed_register_for_instruction(struct register_list_t *list, struct IR_Instruction *instruction, struct IR_Operand *target_operand);

#endif
