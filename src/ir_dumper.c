#include "ir_dumper.h"
#include <stdio.h>

void IR_dump_operand(struct IR_Operand *op) {
    if (!op) {
        printf("null");
        return;
    }

    switch (op->type) {
        case IR_OPERAND_TYPE_VREG:
            printf("%%v%d", op->value.vreg_id);
            break;
        case IR_OPERAND_TYPE_IMM:
            printf("%s", op->value.imm);
            break;
        case IR_OPERAND_TYPE_LABEL:
            printf(".L%p", (void*)op);
            break;
        default:
            printf("???");
            break;
    }
}

void IR_dump_instruction(struct IR_Instruction *inst) {
    if (!inst) return;

    if(IR_INSTRUCTION_TYPE_STORE == inst->type) {
        printf("    store ");
        IR_dump_operand(inst->source_1);
        printf(", ");
        IR_dump_operand(inst->destination);
        printf("\n");
        return;
    }

    printf("    ");

    if (inst->destination) {
        IR_dump_operand(inst->destination);
        printf(" = ");
    }

    switch (inst->type) {
        case IR_INSTRUCTION_TYPE_RET:               printf("ret "); break;

        case IR_INSTRUCTION_TYPE_COPY:              printf("copy "); break;
        case IR_INSTRUCTION_TYPE_ALLOCA:            printf("alloca "); break;
        case IR_INSTRUCTION_TYPE_ADD:               printf("add "); break;
        case IR_INSTRUCTION_TYPE_SUB:               printf("sub "); break;
        case IR_INSTRUCTION_TYPE_MUL:               printf("mul "); break;
        case IR_INSTRUCTION_TYPE_DIV:               printf("div "); break;
        case IR_INSTRUCTION_TYPE_EQUAL_EQUAL:       printf("equal_equal "); break;
        case IR_INSTRUCTION_TYPE_BANG_EQUAL:        printf("bang_equal "); break;
        case IR_INSTRUCTION_TYPE_GREATER_EQUAL:     printf("greater_equal "); break;
        case IR_INSTRUCTION_TYPE_LESS_EQUAL:        printf("less_equal "); break;
        case IR_INSTRUCTION_TYPE_GREATER:           printf("greater "); break;
        case IR_INSTRUCTION_TYPE_LESS:              printf("less "); break;
        case IR_INSTRUCTION_TYPE_STORE:             printf("store "); break;
        case IR_INSTRUCTION_TYPE_LOAD:              printf("load "); break;
        case IR_INSTRUCTION_TYPE_JMP:
            if (inst->data.target_block) {
                printf("jmp .%s ", inst->data.target_block->label);
            } else {
                printf("jmp .NULL_BLOCK ");
            }
            break;

        case IR_INSTRUCTION_TYPE_BR_COND:
            if (inst->data.br_target.false_block && inst->data.br_target.true_block) {
                printf("br cond(");
                IR_dump_operand(inst->source_1);
                printf(") .%s (true), .%s (false) \n", 
                        inst->data.br_target.true_block->label, 
                        inst->data.br_target.false_block->label);
            } else {
                printf("br .INVALID_TARGETS \n");
            }
            return;
        case IR_INSTRUCTION_TYPE_CALL:              
            printf("call @%s ", inst->data.call_info.function_label);
            for(int i = 0;i < inst->data.call_info.arguments->element_count; ++i) {
                struct IR_Operand **param = vector_get(inst->data.call_info.arguments, i);
                IR_dump_operand(*param);
                if(i < inst->data.call_info.arguments->element_count-1)printf(", ");
            }
            printf("\n");
            return;

        case IR_INSTRUCTION_TYPE_PHI:               printf("phi "); break;

        case IR_INSTRUCTION_TYPE_UNARY_MINUS:       printf("unary_minus "); break;
        case IR_INSTRUCTION_TYPE_UNARY_BANG:        printf("unary_bang "); break;
        case IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF:  printf("unary_address_of "); break;
        case IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE: printf("unary_dereference "); break;


        case IR_INSTRUCTION_TYPE_ASM: printf("asm \"%s \"", inst->data.asm_code); break;

        default: printf("unknown_inst "); break;
    }

    if (inst->source_1 && (IR_INSTRUCTION_TYPE_UNARY_MINUS != inst->type && IR_INSTRUCTION_TYPE_UNARY_BANG != inst->type && IR_INSTRUCTION_TYPE_UNARY_ADDRESS_OF != inst->type && IR_INSTRUCTION_TYPE_UNARY_DEREFERENCE != inst->type)) {
        IR_dump_operand(inst->source_1);
    }
    
    if(inst->source_2) {
        printf(", ");
        IR_dump_operand(inst->source_2);
    }

    printf("\n");
}

void IR_dump_block(struct IR_Block *block) {
    if (!block) return;

    if (block->label) {
        printf("%s:\n", block->label);
    } else {
        printf("block_%p:\n", (void*)block);
    }

    struct IR_Instruction *curr = block->entry_instruction;
    while (curr) {
        IR_dump_instruction(curr);
        curr = curr->next;
    }
}

void IR_dump_function(struct IR_Function *func) {
    if (!func) return;

    printf("define fn @%s(", func->label);

    for (int i = 0; i < func->parameter_count; i++) {
        printf("%%v%d", i); 
        if (i < func->parameter_count - 1) {
            printf(", ");
        }
    }

    printf(") {  ; vg_count: %d\n", func->vg_counter);


    struct IR_Block *curr = func->head_block;
    while (curr) {
        IR_dump_block(curr);
        curr = curr->next_in_func;
    }

    printf("}\n\n");
}

void IR_dump_module(struct IR_Module *module) {
    if (!module) return;

    printf("--- Hupeyaszih IR Dump ---\n");
    for (int i = 0; i < module->functions->element_count; ++i) {
        struct IR_Function **fn = vector_get(module->functions, i);
        IR_dump_function(*fn);
    }
    printf("--- End of Dump ---\n");
}

