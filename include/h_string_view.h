#ifndef H_STRING_VIEW_H
#define H_STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct str_view {
    const char *data;
    size_t len;
};

#define SV(cstr) ((struct str_view){ .data = (cstr), .len = sizeof(cstr) - 1 })

#define SV_FMT "%.*s"
#define SV_ARG(sv) ((int)(sv).len), ((sv).data)

static inline struct str_view str_view_make(const char *data, size_t len) {
    return (struct str_view){ .data = data, .len = len };
}
static inline void str_view_free(struct str_view *sv) {
    if (sv == NULL || sv->data == NULL) {
        return;
    }

    free((void *)sv->data);

    sv->data = NULL;
    sv->len = 0;
}

static inline struct str_view str_view_from_cstr(const char *cstr) {
    if (!cstr) return (struct str_view){ NULL, 0 };
    return str_view_make(cstr, strlen(cstr));
}

static inline struct str_view str_view_sub(struct str_view sv, size_t start, size_t len) {
    if (start >= sv.len) return (struct str_view){ NULL, 0 };
    if (start + len > sv.len) len = sv.len - start;
    return str_view_make(sv.data + start, len);
}

static inline bool str_view_eq(struct str_view a, struct str_view b) {
    if (a.len != b.len) return false;
    return memcmp(a.data, b.data, a.len) == 0;
}

static inline bool str_view_eq_cstr(const struct str_view sv, const char *cstr) {
    size_t cstr_len = strlen(cstr);
    if (sv.len != cstr_len) return false;
    return memcmp(sv.data, cstr, sv.len) == 0;
}

static inline struct str_view str_view_trim_left(struct str_view sv) {
    while (sv.len > 0 && (sv.data[0] == ' ' || sv.data[0] == '\t' || sv.data[0] == '\n')) {
        sv.data++;
        sv.len--;
    }
    return sv;
}

static inline int str_view_to_int(struct str_view sv) {
    int result = 0;
    int sign = 1;
    size_t i = 0;

    if (sv.len == 0) return 0;

    if (sv.data[0] == '-') {
        sign = -1;
        i++;
    } else if (sv.data[0] == '+') {
        i++;
    }

    for (; i < sv.len; i++) {
        if (sv.data[i] >= '0' && sv.data[i] <= '9') {
            result = result * 10 + (sv.data[i] - '0');
        } else {
            break;
        }
    }

    return result * sign;
}

static inline bool str_view_is_empty(struct str_view sv) {
    return sv.data == NULL || sv.len == 0;
}
#endif
