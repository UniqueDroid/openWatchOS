/* SPDX-License-Identifier: GPL-2.0-only */
#include "ow_json.h"
#include <string.h>

static const char *ws(const char *p, const char *end) {
    if (!p) return end;   /* a missing value: behaves like the end of the text */
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static const char *skip_string(const char *p, const char *end) {   /* p at the opening quote */
    for (p++; p < end; p++) {
        if (*p == '\\') { p++; continue; }
        if (*p == '"') return p + 1;
    }
    return 0;
}

const char *ow_json_skip(const char *p, const char *end) {
    p = ws(p, end);
    if (p >= end) return 0;
    if (*p == '"') return skip_string(p, end);
    if (*p == '{' || *p == '[') {
        int level = 0;
        for (; p < end; p++) {
            if (*p == '"') { p = skip_string(p, end); if (!p) return 0; p--; continue; }
            if (*p == '{' || *p == '[') level++;
            if ((*p == '}' || *p == ']') && --level == 0) return p + 1;
        }
        return 0;
    }
    while (p < end && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') p++;
    return p;
}

const char *ow_json_get(const char *p, const char *end, const char *key) {
    if (!p) return 0;
    p = ws(p, end);
    if (p >= end || *p != '{') return 0;
    size_t klen = strlen(key);
    p++;
    for (;;) {
        p = ws(p, end);
        if (p >= end || *p == '}') return 0;
        if (*p != '"') return 0;
        const char *k = p + 1, *ke = skip_string(p, end);
        if (!ke) return 0;
        p = ws(ke, end);
        if (p >= end || *p != ':') return 0;
        p = ws(p + 1, end);
        if ((size_t)(ke - 1 - k) == klen && !memcmp(k, key, klen)) return p;
        p = ow_json_skip(p, end);
        if (!p) return 0;
        p = ws(p, end);
        if (p < end && *p == ',') p++;
    }
}

static int put_utf8(char *out, int at, int cap, unsigned c) {
    char b[4]; int n;
    if (c < 0x80) { b[0] = (char)c; n = 1; }
    else if (c < 0x800) { b[0] = (char)(0xC0 | c >> 6); b[1] = (char)(0x80 | (c & 63)); n = 2; }
    else if (c < 0x10000) { b[0] = (char)(0xE0 | c >> 12); b[1] = (char)(0x80 | (c >> 6 & 63)); b[2] = (char)(0x80 | (c & 63)); n = 3; }
    else { b[0] = (char)(0xF0 | c >> 18); b[1] = (char)(0x80 | (c >> 12 & 63)); b[2] = (char)(0x80 | (c >> 6 & 63)); b[3] = (char)(0x80 | (c & 63)); n = 4; }
    if (at + n >= cap) return at;
    memcpy(out + at, b, (size_t)n);
    return at + n;
}

static int hex4(const char *p, const char *end, unsigned *v) {
    if (end - p < 4) return 0;
    *v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i]; unsigned d = c >= '0' && c <= '9' ? (unsigned)(c - '0') : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
                     : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 99u;
        if (d > 15u) return 0;
        *v = *v << 4 | d;
    }
    return 1;
}

int ow_json_str(const char *v, const char *end, char *out, int cap) {
    if (cap > 0) out[0] = 0;
    v = ws(v, end);
    if (v >= end || *v != '"' || cap <= 0) return -1;
    int n = 0;
    for (v++; v < end && *v != '"'; v++) {
        unsigned c = (unsigned char)*v;
        if (c == '\\' && v + 1 < end) {
            c = (unsigned char)*++v;
            if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r';
            else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f';
            else if (c == 'u') {
                if (!hex4(v + 1, end, &c)) return -1;
                v += 4;
                if (c >= 0xD800 && c < 0xDC00 && v + 6 < end && v[1] == '\\' && v[2] == 'u') {
                    unsigned lo;
                    if (hex4(v + 3, end, &lo) && lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); v += 6; }
                }
                n = put_utf8(out, n, cap, c);
                continue;
            }
        }
        if (n + 1 < cap) out[n++] = (char)c;
    }
    out[n] = 0;
    return n;
}

long ow_json_num(const char *v, const char *end, long fallback) {
    v = ws(v, end);
    if (v >= end) return fallback;
    int neg = *v == '-';
    if (neg) v++;
    if (v >= end || *v < '0' || *v > '9') return fallback;
    long x = 0;
    while (v < end && *v >= '0' && *v <= '9') x = x * 10 + (*v++ - '0');
    return neg ? -x : x;
}

int ow_json_bool(const char *v, const char *end, int fallback) {
    v = ws(v, end);
    if (end - v >= 4 && !memcmp(v, "true", 4)) return 1;
    if (end - v >= 5 && !memcmp(v, "false", 5)) return 0;
    return fallback;
}

const char *ow_json_first(const char *v, const char *end) {
    v = ws(v, end);
    if (v >= end || *v != '[') return 0;
    v = ws(v + 1, end);
    return v < end && *v != ']' ? v : 0;
}

const char *ow_json_next(const char *e, const char *end) {
    if (!e) return 0;
    e = ow_json_skip(e, end);
    if (!e) return 0;
    e = ws(e, end);
    if (e >= end || *e != ',') return 0;
    e = ws(e + 1, end);
    return e < end ? e : 0;
}
