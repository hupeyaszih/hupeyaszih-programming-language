#ifndef H_FlAGS_FUNCTION_H
#define H_FlAGS_FUNCTION_H

#include <stdint.h>
#define FUNC_FLAG_IS_PURE (1 << 0)

static inline int function_flags_get_is_pure_function(int8_t flags) {
    return flags & FUNC_FLAG_IS_PURE;
}

static inline void function_flags_set_is_pure_function(int8_t *flags, int is_pure) {
    if(0 == is_pure) {
        *flags &= ~FUNC_FLAG_IS_PURE;
    }else {
        *flags |= FUNC_FLAG_IS_PURE;
    }
}
#endif
