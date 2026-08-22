#ifndef H_STRING_VIEW_H
#define H_STRING_VIEW_H

#include "h_arena.h"
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

static inline struct str_view str_view_from_cstr(struct arena *arena, const char *cstr) {
    if (!cstr) return (struct str_view){ NULL, 0 };
    
    size_t len = strlen(cstr);
    
    char *copy = arena_alloc(arena, len + 1); 
    memcpy(copy, cstr, len + 1);
    
    return str_view_make(copy, len);
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

static inline int64_t str_view_to_char(struct str_view sv) {
    if (sv.len == 0 || sv.data == NULL) return 0;

    size_t start = 0;
    size_t end = sv.len;

    if (sv.data[0] == '\'') start++;
    if (end > start && sv.data[end - 1] == '\'') end--;

    size_t len = end - start;
    if (len == 0) return 0;

    if (sv.data[start] == '\\' && len >= 2) {
        switch (sv.data[start + 1]) {
            case 'n':  return '\n';
            case 't':  return '\t';
            case 'r':  return '\r';
            case '0':  return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            case '\"': return '\"';
            default:   return (int64_t)sv.data[start + 1];
        }
    }

    return (int64_t)(unsigned char)sv.data[start];
}

static inline struct str_view str_view_from_char_literal(struct str_view sv) {
    int64_t ascii_val = str_view_to_char(sv);
    
    static const struct str_view ASCII_SV_TABLE[256] = {
        { .data = "0", .len = 1 },
        { .data = "1", .len = 1 },
        { .data = "2", .len = 1 },
        { .data = "3", .len = 1 },
        { .data = "4", .len = 1 },
        { .data = "5", .len = 1 },
        { .data = "6", .len = 1 },
        { .data = "7", .len = 1 },
        { .data = "8", .len = 1 },
        { .data = "9", .len = 1 },
        { .data = "10", .len = 2 },
        { .data = "11", .len = 2 },
        { .data = "12", .len = 2 },
        { .data = "13", .len = 2 },
        { .data = "14", .len = 2 },
        { .data = "15", .len = 2 },
        { .data = "16", .len = 2 },
        { .data = "17", .len = 2 },
        { .data = "18", .len = 2 },
        { .data = "19", .len = 2 },
        { .data = "20", .len = 2 },
        { .data = "21", .len = 2 },
        { .data = "22", .len = 2 },
        { .data = "23", .len = 2 },
        { .data = "24", .len = 2 },
        { .data = "25", .len = 2 },
        { .data = "26", .len = 2 },
        { .data = "27", .len = 2 },
        { .data = "28", .len = 2 },
        { .data = "29", .len = 2 },
        { .data = "30", .len = 2 },
        { .data = "31", .len = 2 },
        { .data = "32", .len = 2 },
        { .data = "33", .len = 2 },
        { .data = "34", .len = 2 },
        { .data = "35", .len = 2 },
        { .data = "36", .len = 2 },
        { .data = "37", .len = 2 },
        { .data = "38", .len = 2 },
        { .data = "39", .len = 2 },
        { .data = "40", .len = 2 },
        { .data = "41", .len = 2 },
        { .data = "42", .len = 2 },
        { .data = "43", .len = 2 },
        { .data = "44", .len = 2 },
        { .data = "45", .len = 2 },
        { .data = "46", .len = 2 },
        { .data = "47", .len = 2 },
        { .data = "48", .len = 2 },
        { .data = "49", .len = 2 },
        { .data = "50", .len = 2 },
        { .data = "51", .len = 2 },
        { .data = "52", .len = 2 },
        { .data = "53", .len = 2 },
        { .data = "54", .len = 2 },
        { .data = "55", .len = 2 },
        { .data = "56", .len = 2 },
        { .data = "57", .len = 2 },
        { .data = "58", .len = 2 },
        { .data = "59", .len = 2 },
        { .data = "60", .len = 2 },
        { .data = "61", .len = 2 },
        { .data = "62", .len = 2 },
        { .data = "63", .len = 2 },
        { .data = "64", .len = 2 },
        { .data = "65", .len = 2 },
        { .data = "66", .len = 2 },
        { .data = "67", .len = 2 },
        { .data = "68", .len = 2 },
        { .data = "69", .len = 2 },
        { .data = "70", .len = 2 },
        { .data = "71", .len = 2 },
        { .data = "72", .len = 2 },
        { .data = "73", .len = 2 },
        { .data = "74", .len = 2 },
        { .data = "75", .len = 2 },
        { .data = "76", .len = 2 },
        { .data = "77", .len = 2 },
        { .data = "78", .len = 2 },
        { .data = "79", .len = 2 },
        { .data = "80", .len = 2 },
        { .data = "81", .len = 2 },
        { .data = "82", .len = 2 },
        { .data = "83", .len = 2 },
        { .data = "84", .len = 2 },
        { .data = "85", .len = 2 },
        { .data = "86", .len = 2 },
        { .data = "87", .len = 2 },
        { .data = "88", .len = 2 },
        { .data = "89", .len = 2 },
        { .data = "90", .len = 2 },
        { .data = "91", .len = 2 },
        { .data = "92", .len = 2 },
        { .data = "93", .len = 2 },
        { .data = "94", .len = 2 },
        { .data = "95", .len = 2 },
        { .data = "96", .len = 2 },
        { .data = "97", .len = 2 },
        { .data = "98", .len = 2 },
        { .data = "99", .len = 2 },
        { .data = "100", .len = 3 },
        { .data = "101", .len = 3 },
        { .data = "102", .len = 3 },
        { .data = "103", .len = 3 },
        { .data = "104", .len = 3 },
        { .data = "105", .len = 3 },
        { .data = "106", .len = 3 },
        { .data = "107", .len = 3 },
        { .data = "108", .len = 3 },
        { .data = "109", .len = 3 },
        { .data = "110", .len = 3 },
        { .data = "111", .len = 3 },
        { .data = "112", .len = 3 },
        { .data = "113", .len = 3 },
        { .data = "114", .len = 3 },
        { .data = "115", .len = 3 },
        { .data = "116", .len = 3 },
        { .data = "117", .len = 3 },
        { .data = "118", .len = 3 },
        { .data = "119", .len = 3 },
        { .data = "120", .len = 3 },
        { .data = "121", .len = 3 },
        { .data = "122", .len = 3 },
        { .data = "123", .len = 3 },
        { .data = "124", .len = 3 },
        { .data = "125", .len = 3 },
        { .data = "126", .len = 3 },
        { .data = "127", .len = 3 },
        { .data = "128", .len = 3 },
        { .data = "129", .len = 3 },
        { .data = "130", .len = 3 },
        { .data = "131", .len = 3 },
        { .data = "132", .len = 3 },
        { .data = "133", .len = 3 },
        { .data = "134", .len = 3 },
        { .data = "135", .len = 3 },
        { .data = "136", .len = 3 },
        { .data = "137", .len = 3 },
        { .data = "138", .len = 3 },
        { .data = "139", .len = 3 },
        { .data = "140", .len = 3 },
        { .data = "141", .len = 3 },
        { .data = "142", .len = 3 },
        { .data = "143", .len = 3 },
        { .data = "144", .len = 3 },
        { .data = "145", .len = 3 },
        { .data = "146", .len = 3 },
        { .data = "147", .len = 3 },
        { .data = "148", .len = 3 },
        { .data = "149", .len = 3 },
        { .data = "150", .len = 3 },
        { .data = "151", .len = 3 },
        { .data = "152", .len = 3 },
        { .data = "153", .len = 3 },
        { .data = "154", .len = 3 },
        { .data = "155", .len = 3 },
        { .data = "156", .len = 3 },
        { .data = "157", .len = 3 },
        { .data = "158", .len = 3 },
        { .data = "159", .len = 3 },
        { .data = "160", .len = 3 },
        { .data = "161", .len = 3 },
        { .data = "162", .len = 3 },
        { .data = "163", .len = 3 },
        { .data = "164", .len = 3 },
        { .data = "165", .len = 3 },
        { .data = "166", .len = 3 },
        { .data = "167", .len = 3 },
        { .data = "168", .len = 3 },
        { .data = "169", .len = 3 },
        { .data = "170", .len = 3 },
        { .data = "171", .len = 3 },
        { .data = "172", .len = 3 },
        { .data = "173", .len = 3 },
        { .data = "174", .len = 3 },
        { .data = "175", .len = 3 },
        { .data = "176", .len = 3 },
        { .data = "177", .len = 3 },
        { .data = "178", .len = 3 },
        { .data = "179", .len = 3 },
        { .data = "180", .len = 3 },
        { .data = "181", .len = 3 },
        { .data = "182", .len = 3 },
        { .data = "183", .len = 3 },
        { .data = "184", .len = 3 },
        { .data = "185", .len = 3 },
        { .data = "186", .len = 3 },
        { .data = "187", .len = 3 },
        { .data = "188", .len = 3 },
        { .data = "189", .len = 3 },
        { .data = "190", .len = 3 },
        { .data = "191", .len = 3 },
        { .data = "192", .len = 3 },
        { .data = "193", .len = 3 },
        { .data = "194", .len = 3 },
        { .data = "195", .len = 3 },
        { .data = "196", .len = 3 },
        { .data = "197", .len = 3 },
        { .data = "198", .len = 3 },
        { .data = "199", .len = 3 },
        { .data = "200", .len = 3 },
        { .data = "201", .len = 3 },
        { .data = "202", .len = 3 },
        { .data = "203", .len = 3 },
        { .data = "204", .len = 3 },
        { .data = "205", .len = 3 },
        { .data = "206", .len = 3 },
        { .data = "207", .len = 3 },
        { .data = "208", .len = 3 },
        { .data = "209", .len = 3 },
        { .data = "210", .len = 3 },
        { .data = "211", .len = 3 },
        { .data = "212", .len = 3 },
        { .data = "213", .len = 3 },
        { .data = "214", .len = 3 },
        { .data = "215", .len = 3 },
        { .data = "216", .len = 3 },
        { .data = "217", .len = 3 },
        { .data = "218", .len = 3 },
        { .data = "219", .len = 3 },
        { .data = "220", .len = 3 },
        { .data = "221", .len = 3 },
        { .data = "222", .len = 3 },
        { .data = "223", .len = 3 },
        { .data = "224", .len = 3 },
        { .data = "225", .len = 3 },
        { .data = "226", .len = 3 },
        { .data = "227", .len = 3 },
        { .data = "228", .len = 3 },
        { .data = "229", .len = 3 },
        { .data = "230", .len = 3 },
        { .data = "231", .len = 3 },
        { .data = "232", .len = 3 },
        { .data = "233", .len = 3 },
        { .data = "234", .len = 3 },
        { .data = "235", .len = 3 },
        { .data = "236", .len = 3 },
        { .data = "237", .len = 3 },
        { .data = "238", .len = 3 },
        { .data = "239", .len = 3 },
        { .data = "240", .len = 3 },
        { .data = "241", .len = 3 },
        { .data = "242", .len = 3 },
        { .data = "243", .len = 3 },
        { .data = "244", .len = 3 },
        { .data = "245", .len = 3 },
        { .data = "246", .len = 3 },
        { .data = "247", .len = 3 },
        { .data = "248", .len = 3 },
        { .data = "249", .len = 3 },
        { .data = "250", .len = 3 },
        { .data = "251", .len = 3 },
        { .data = "252", .len = 3 },
        { .data = "253", .len = 3 },
        { .data = "254", .len = 3 },
        { .data = "255", .len = 3 }
    };
    if (ascii_val >= 0 && ascii_val <= 255) {
        return ASCII_SV_TABLE[ascii_val];
    }

    return SV("0");
}

static inline int str_view_to_binary_int(struct str_view sv, size_t start_idx, int sign) {
    int result = 0;
    for (size_t i = start_idx; i < sv.len; i++) {
        if (sv.data[i] == '0' || sv.data[i] == '1') {
            result = (result << 1) + (sv.data[i] - '0');
        } else {
            break;
        }
    }
    return result * sign;
}

static inline int str_view_to_hex_int(struct str_view sv, size_t start_idx, int sign) {
    int result = 0;
    for (size_t i = start_idx; i < sv.len; i++) {
        char c = sv.data[i];
        int val = 0;
        if (c >= '0' && c <= '9') {
            val = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            val = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            val = c - 'A' + 10;
        } else {
            break;
        }
        result = (result << 4) + val;
    }
    return result * sign;
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

    if (i + 1 < sv.len && sv.data[i] == '0' && (sv.data[i+1] == 'x' || sv.data[i+1] == 'X')) {
        return str_view_to_hex_int(sv, i + 2, sign);
    }

    if (i + 1 < sv.len && sv.data[i] == '0' && (sv.data[i+1] == 'b' || sv.data[i+1] == 'B')) {
        return str_view_to_binary_int(sv, i + 2, sign);
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
