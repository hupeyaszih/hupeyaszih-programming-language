#ifndef BACKEND_X86_64_LINUX_H
#define BACKEND_X86_64_LINUX_H


struct register_t;
enum register_size;

struct codegen_build_target_t *x86_64_linux_create_build_target();

struct register_list_t *x86_64_linux_create_register_list(struct codegen_build_target_t *arch);

const char *x86_64_linux_get_register_name(struct register_list_t *list, struct register_t *reg, enum register_size size);
struct register_t *x86_64_linux_get_best_available_register(struct register_list_t *list);

#endif
