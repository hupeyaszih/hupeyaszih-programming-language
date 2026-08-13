#include "backend/codegen_utils.h"
#include "backend/codegen.h"
#include "core/ir_gen.h"
#include "h_bitset.h"
#include "h_vector.h"
#include <stdbool.h>
#include <stdio.h>

struct node_t {
    struct register_t *reg;
    struct IR_Operand *arg;
    struct register_t *src_reg;
    struct register_t *temp_reserved;
    struct bitset_t *in;
    struct bitset_t *out;

    int in_degree;
    int out_degree;

    bool is_visited;
    bool is_done;
};

static inline void build_graph(struct node_t *nodes, int node_count) {
    for(int i = 0;i < node_count; ++i) {
        struct node_t *node = nodes + i;
        for(int k = 0;k < node_count; ++k) {
            struct node_t *target_node = nodes + k;
            if(node->reg->id == target_node->reg->id) continue;

            if(bitset_test(node->in, target_node->reg->id)) {
                bitset_set(target_node->out, node->reg->id);
                ++target_node->out_degree;
            }
        }
    }
}

void codegen_utils_emit_call_args(struct arena *arena, struct codegen_context_t *context, struct vector_t *arguments, struct vector_t *out_regs, int arg_count) {
    if(!context || !arguments || arg_count <= 0) return;
    struct codegen_build_target_t *build_target = context->build_target;

    int args_via_registers = arg_count;
    struct node_t nodes[args_via_registers];

    for(int i = 0; i < args_via_registers; ++i) {
        struct IR_Operand *arg = *(struct IR_Operand **) vector_get(arguments, i);
        struct register_t *reg = *(struct register_t **) vector_get(out_regs, i);

        struct node_t *node = nodes + i;
        node->arg = arg;
        node->src_reg = (arg->type == IR_OPERAND_TYPE_VREG) ? arg->data.vreg.reg : NULL;
        node->temp_reserved = NULL;
        node->reg = reg;

        node->is_visited = false;
        node->is_done = false;
        node->in_degree = 0;
        node->out_degree = 0;

        node->in  = bitset_create(arena, build_target->registers->register_count);
        node->out = bitset_create(arena, build_target->registers->register_count);

        if(IR_OPERAND_TYPE_VREG == arg->type) {
            bitset_set(node->in, arg->data.vreg.reg->id);
            ++node->in_degree;
        }
    }

    build_graph(nodes, args_via_registers);

    int remaining = args_via_registers;
    while (remaining > 0) {
        bool progress = false;

        for(int i = 0; i < args_via_registers; ++i) {
            struct node_t *node = nodes + i;
            if(node->is_done) continue;

            if(0 == node->out_degree) {
                if (node->src_reg != NULL) {
                    if (node->src_reg->id != node->reg->id) {
                        build_target->emit_mov_reg_to_reg(context, node->reg, node->src_reg, REGISTER_SIZE_64);
                    }
                } else {
                    build_target->emit_mov_operand_to_reg(context, node->reg, node->arg);
                }

                if (node->temp_reserved != NULL) {
                    struct register_t *res_reg = node->temp_reserved;
                    node->temp_reserved = NULL;

                    bool still_used = false;
                    for (int k = 0; k < args_via_registers; ++k) {
                        if (!nodes[k].is_done && nodes[k].src_reg == res_reg) {
                            still_used = true;
                            break;
                        }
                    }

                    if (!still_used) {
                        build_target->set_free_reserved_register(build_target->registers, res_reg);
                    }
                }

                node->is_done = true;
                --remaining;
                progress = true;

                for (int k = 0; k < args_via_registers; ++k) {
                    if (nodes[k].is_done) continue;
                    if (bitset_test(node->in, nodes[k].reg->id)) {
                        if (nodes[k].out_degree > 0) --nodes[k].out_degree;
                    }
                }
            }
        }

        if(progress) continue;

        for (int i = 0; i < args_via_registers; ++i) {
            struct node_t *node = nodes + i;
            if (node->is_done) continue;

            struct register_t *reserved = build_target->get_available_reserved_register(build_target->registers);

            build_target->emit_mov_reg_to_reg(context, reserved, node->reg, REGISTER_SIZE_64);

            for (int k = 0; k < args_via_registers; ++k) {
                if (nodes[k].is_done) continue;
                if (nodes[k].src_reg != NULL && nodes[k].src_reg->id == node->reg->id) {
                    nodes[k].src_reg = reserved;
                    nodes[k].temp_reserved = reserved;
                }
            }

            node->out_degree = 0;
            break; 
        }
    }
}
