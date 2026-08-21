#include "core/ir_dumper.h"
#include "core/ir_gen.h"
#include "core/symbol_table.h"
#include "h_vector.h"
#include <stdio.h>

static inline void IR_dump_type_info(const struct type_info *type_info) {
    printf(SV_FMT, SV_ARG(type_info->name));

    for(int i = 0;i < type_info->pointer_level; ++i) {
        printf("*");
    }
}

static inline void IR_dump_operand(const struct IR_Operand *restrict operand) {
    if(!operand) return;
    switch (operand->type) {
        case IR_OPERAND_TYPE_IMM: {
            IR_dump_type_info(operand->type_info);
            printf(" " SV_FMT "", SV_ARG(operand->data.imm_value));
            break;
        }case IR_OPERAND_TYPE_VREG: {
            IR_dump_type_info(operand->type_info);

            printf(" v%d", operand->data.vreg.vreg_id);
            break;
        }case IR_OPERAND_TYPE_UNDEFINED: {
            printf("undefined");
            break;
        }case IR_OPERAND_TYPE_LABEL: {
            printf(SV_FMT ":", SV_ARG(operand->data.mangled_label_name));
            break;
        }case IR_OPERAND_TYPE_STACK_SLOT: {
            if(!operand->data.slot.stack_slot) break;
            printf("[%d, size: %ld]", operand->data.slot.stack_slot->stack_offset, operand->data.slot.stack_slot->type->size);
            break;
        }case IR_OPERAND_TYPE_GLOBAL: {
            IR_dump_type_info(operand->type_info);
            printf(" @" SV_FMT, SV_ARG(operand->data.global.name));
            break;
        }
    }
}

void IR_dump_module(const struct IR_Module *restrict module) {
    printf("module %s globals[\n", module->name);
    for(int i = 0;i < module->globals->element_count; ++i) {
        struct IR_Operand *global = *(struct IR_Operand **) vector_get(module->globals, i);
        IR_dump_type_info(global->type_info);
        printf(" " SV_FMT " = " SV_FMT " ", SV_ARG(global->data.global.name), SV_ARG(global->data.global.value));
        printf("\n");
    }
    printf("]\n");

    for(int i = 0;i < module->functions->element_count; ++i) {
        struct IR_Function *function = *(struct IR_Function **) vector_get(module->functions, i);
        IR_dump_function(function);
    }
}

static void IR_dump_function(const struct IR_Function *function) {
    printf("\n");
    // printf("fn %s, vreg count; %ld\n", function->name, function->unique_vregs->element_count);
    printf("fn " SV_FMT " (", SV_ARG(function->name));
    for(int p = 0;p < function->parameter_count; ++p) {
        struct IR_Operand *param = *(struct IR_Operand **) vector_get(function->parameters,p);
        IR_dump_operand(param);
        if(p != function->parameter_count-1) printf(", ");
    }
    printf(")");
    printf(" , vreg count; %ld", function->unique_vregs->element_count);
    printf(" , stack size; %d\n", function->stack_size);
    printf("{\n");

    struct IR_Block *curr = function->head_block;
    while (NULL != curr) {
        IR_dump_block(curr);
        curr = curr->next;
    }

    printf("}\n");
    printf("\n");
}
static void IR_dump_block(const struct IR_Block *block) {
    printf("\n");
    printf(SV_FMT " ", SV_ARG(block->mangled_name));
    if(block->params->element_count > 0) {
        printf("(");
        for(int i = 0;i < block->params->element_count; ++i) {
            struct IR_Operand *op = *(struct IR_Operand **) vector_get(block->params, i);
            IR_dump_operand(op);
            if(i != block->params->element_count-1) printf(", ");
        }
        printf(")");
    }
    printf(":\n");
    struct IR_Instruction *curr = block->head_instruction;
    while(NULL != curr) {
        IR_dump_instruction(curr);
        curr = curr->next;
    }
}

static inline void IR_dump_alu(const struct IR_Instruction *instruction, char *name) {
    IR_dump_operand(instruction->operands.triple_operands.destination);
    printf(" = %s ", name);
    IR_dump_operand(instruction->operands.triple_operands.source_1);
    printf(" , ");
    IR_dump_operand(instruction->operands.triple_operands.source_2);
    printf("\n");
}

static void IR_dump_instruction(const struct IR_Instruction *instruction) {
    switch (instruction->type) {
        case IR_INSTRUCTION_TYPE_BITWISE_AND:   {IR_dump_alu(instruction, "and");   break;}
        case IR_INSTRUCTION_TYPE_BITWISE_OR:   {IR_dump_alu(instruction, "or");   break;}
        case IR_INSTRUCTION_TYPE_BITWISE_XOR:   {IR_dump_alu(instruction, "xor");   break;}
        case IR_INSTRUCTION_TYPE_SHL:   {IR_dump_alu(instruction, "shl");   break;}
        case IR_INSTRUCTION_TYPE_SHR:   {IR_dump_alu(instruction, "shr");   break;}

        case IR_INSTRUCTION_TYPE_PLUS:   {IR_dump_alu(instruction, "add");   break;}
        case IR_INSTRUCTION_TYPE_MINUS:  {IR_dump_alu(instruction, "minus"); break;}
        case IR_INSTRUCTION_TYPE_DIVIDE: {IR_dump_alu(instruction, "div");   break;}
        case IR_INSTRUCTION_TYPE_MUL:    {IR_dump_alu(instruction, "mul");   break;}

        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:   {IR_dump_alu(instruction, "equal_equal");   break;}
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:    {IR_dump_alu(instruction, "bang_equal"); break;}
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL: {IR_dump_alu(instruction, "greater_equal");   break;}
        case IR_INSTRUCTION_TYPE_GREATER:       {IR_dump_alu(instruction, "greater");   break;}
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:    {IR_dump_alu(instruction, "less_equal");   break;}
        case IR_INSTRUCTION_TYPE_LESS:          {IR_dump_alu(instruction, "less"); break;}

        case IR_INSTRUCTION_TYPE_ALLOCA:{
            IR_dump_operand(instruction->operands.alloca.destination);
            printf(" = alloca ");
            IR_dump_type_info(instruction->operands.alloca.type_info);
            printf(" (size: %ld)", instruction->operands.alloca.type_info->size);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_STORE:{
            printf("store ");
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(", ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_LOAD:{
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = load ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_MOV: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = mov ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_CAST: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = cast ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_BR: {
            printf("br ");
            IR_dump_operand(instruction->operands.br.condition);
            printf("; (false) " SV_FMT " , (true) " SV_FMT "\n", SV_ARG(instruction->operands.br.false_block->mangled_name), SV_ARG(instruction->operands.br.true_block->mangled_name));
            break;
        }case IR_INSTRUCTION_TYPE_JMP: {
            printf("jmp ");
            printf(SV_FMT, SV_ARG(instruction->operands.jmp.target_block->mangled_name));
            struct vector_t *args = instruction->operands.jmp.args;
            if(args && args->element_count > 0) {
                printf(" (");
                for(int i = 0;i < args->element_count; ++i) {
                    struct IR_Operand *op = *(struct IR_Operand **) vector_get(args, i);
                    IR_dump_operand(op);
                    
                    if(i != args->element_count - 1) printf(" , ");
                }
                printf(" )");
            }
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_CALL: {
            IR_dump_operand(instruction->operands.call.return_val);
            printf(" = call " SV_FMT "(", SV_ARG(instruction->operands.call.target_function->name));
            for(int i = 0;i < instruction->operands.call.arguments->element_count; ++i) {
                struct IR_Operand *arg = *(struct IR_Operand **) vector_get(instruction->operands.call.arguments, i);
                IR_dump_operand(arg);
                if(i != instruction->operands.call.arguments->element_count-1) printf(", ");
            }

            printf(")\n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = address_of ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = dereference ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_BANG: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = unary_bang ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_MINUS: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = unary_minus ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_UNARY_NOT: {
            IR_dump_operand(instruction->operands.double_operands.destination);
            printf(" = unary_not ");
            IR_dump_operand(instruction->operands.double_operands.source_1);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_RET: {
            printf("ret ");
            IR_dump_operand(instruction->operands.ret.return_value);
            printf("\n");
            break;
        }case IR_INSTRUCTION_TYPE_NOP: break;
        default: printf("%d\n", instruction->type);
    }
}
