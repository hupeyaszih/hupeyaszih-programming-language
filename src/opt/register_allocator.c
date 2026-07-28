#include "opt/register_allocator.h"
#include "core/ir_gen.h"
#include "h_vector.h"
#include <stdio.h>
#include <stdlib.h>

struct register_allocator_t *register_allocator_create_register_allocator() {
    struct register_allocator_t *allocator = calloc(1, sizeof(struct register_allocator_t));
    return allocator;
}
void register_allocator_delete_register_allocator(struct register_allocator_t **register_allocator) {
    free(*register_allocator);
}

void register_allocator_run_allocator(struct register_allocator_t *allocator, struct IR_Function *function) {
    if(!function || !allocator) return;
    int vreg_count = function->unique_vregs->element_count;
    struct vector_t *vregs = function->unique_vregs;

    for(int i = 0;i < vreg_count; ++i) {
        struct IR_Operand *operand = *(struct IR_Operand **) vector_get(vregs, i);
        int use_score = operand->data.vreg.live_interval.use_score;
        int live_length = operand->data.vreg.live_interval.end - operand->data.vreg.live_interval.start;

        int weight = 0;
        if(live_length == 0) {
            weight = 1;
        }else {
            weight = use_score / live_length;
        }

        operand->data.vreg.live_interval.weight = weight;
    }

}
