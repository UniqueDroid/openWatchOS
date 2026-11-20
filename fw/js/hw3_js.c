/* SPDX-License-Identifier: GPL-2.0-only */
#include "hw3_js.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mquickjs.h"

/* Natives named by the standard library table. Nothing is printed or loaded on the watch yet. */
static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)this_val; (void)argc; (void)argv; JS_GC(ctx); return JS_UNDEFINED; }
JSValue js_date_constructor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)this_val; (void)argc; (void)argv; return JS_NewInt32(ctx, 0); }
static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)this_val; (void)argc; (void)argv; return JS_NewInt32(ctx, 0); }

/* fillRect(x1, y1, x2, y2, 0xRRGGBB) */
static JSValue js_fillRect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int v[5], i;
    (void)this_val;
    if (argc < 5) return JS_UNDEFINED;
    for (i = 0; i < 5; i++) if (JS_ToInt32(ctx, &v[i], argv[i])) return JS_EXCEPTION;
    hw3_js_host_rect(v[0], v[1], v[2], v[3], (uint32_t)v[4] & 0xffffffu);
    return JS_UNDEFINED;
}

/* drawText(x, y, string): the string goes out as UTF-16; anything past HW3_JS_TEXT - 1 characters is cut. */
static JSValue js_drawText(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int x, y; unsigned n = 0; uint16_t out[HW3_JS_TEXT];
    JSCStringBuf buffer; const unsigned char *s;
    (void)this_val;
    if (argc < 3) return JS_UNDEFINED;
    if (JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1])) return JS_EXCEPTION;
    s = (const unsigned char *)JS_ToCString(ctx, argv[2], &buffer);
    if (!s) return JS_EXCEPTION;
    while (*s && n < HW3_JS_TEXT - 1u) {
        uint32_t c = *s++; unsigned more = c >= 0xf0u ? 3u : c >= 0xe0u ? 2u : c >= 0xc0u ? 1u : 0u;
        if (more) c &= 0x3fu >> more;
        while (more-- && (*s & 0xc0u) == 0x80u) c = (c << 6) | (*s++ & 0x3fu);
        out[n++] = c > 0xffffu ? (uint16_t)'?' : (uint16_t)c;      /* the watch font has no characters past U+FFFF */
    }
    out[n] = 0;
    hw3_js_host_text(x, y, out);
    return JS_UNDEFINED;
}

void hw3_js_calendar(uint32_t seconds, int out[7]) {
    static const uint8_t length[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t days = seconds / 86400u, rest = seconds % 86400u, year = 2000, month = 0, n;
    out[6] = (int)((days + 5u) % 7u + 1u);                     /* 2000-01-01 was a Saturday */
    for (;;) {                                                 /* 2000..2099: every fourth year is a leap year */
        n = (year % 4u == 0) ? 366u : 365u;
        if (days < n) break;
        days -= n; year++;
    }
    for (;;) {
        n = length[month] + ((month == 1u && year % 4u == 0) ? 1u : 0u);
        if (days < n) break;
        days -= n; month++;
    }
    out[0] = (int)year; out[1] = (int)month + 1; out[2] = (int)days + 1;
    out[3] = (int)(rest / 3600u); out[4] = (int)(rest % 3600u / 60u); out[5] = (int)(rest % 60u);
}

static int denied(JSContext *ctx, unsigned permission) {
    if (hw3_js_host_allowed(permission)) return 0;
    JS_ThrowTypeError(ctx, "permission");
    return 1;
}

/* time() -> [year, month, day, hour, minute, second, weekday] */
static JSValue js_time(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int part[7], i; JSValue array; JSGCRef array_ref;
    (void)this_val; (void)argc; (void)argv;
    if (denied(ctx, HW3_JS_PERM_TIME)) return JS_EXCEPTION;
    hw3_js_calendar(hw3_js_host_seconds(), part);
    array = JS_NewArray(ctx, 7);
    if (JS_IsException(array)) return array;
    JS_PUSH_VALUE(ctx, array);
    for (i = 0; i < 7; i++) JS_SetPropertyUint32(ctx, array_ref.val, (uint32_t)i, JS_NewInt32(ctx, part[i]));
    JS_POP_VALUE(ctx, array);
    return array;
}

static JSValue js_setTimer(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int ms = 0;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_TIMER)) return JS_EXCEPTION;
    if (argc > 0 && JS_ToInt32(ctx, &ms, argv[0])) return JS_EXCEPTION;
    hw3_js_host_timer(ms <= 0 ? 0u : ms < 100 ? 100u : ms > 60000 ? 60000u : (unsigned)ms);
    return JS_UNDEFINED;
}

static JSValue js_vibrate(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int ms = 0;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_VIBRATE)) return JS_EXCEPTION;
    if (argc > 0 && JS_ToInt32(ctx, &ms, argv[0])) return JS_EXCEPTION;
    hw3_js_host_vibrate(ms < 10 ? 10u : ms > 2000 ? 2000u : (unsigned)ms);
    return JS_UNDEFINED;
}

/* noise() / noise(1) -> tenths of dB, -1, -2 */
static JSValue js_noise(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int restart = 0;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_MICROPHONE)) return JS_EXCEPTION;
    if (argc > 0 && JS_ToInt32(ctx, &restart, argv[0])) return JS_EXCEPTION;
    return JS_NewInt32(ctx, hw3_js_host_noise(restart != 0));
}

/* peek(address) -> RAM word (read only, RAM range checked by the host) */
/* launch(hex) -> only returns on errors (negative number); the OWLP program never comes back */
static JSValue js_launch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JSCStringBuf buffer; const char *text; size_t length;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_BOOT)) return JS_EXCEPTION;
    if (argc < 1) return JS_NewInt32(ctx, -1);
    text = JS_ToCStringLen(ctx, &length, argv[0], &buffer);
    if (!text) return JS_EXCEPTION;
    return JS_NewInt32(ctx, hw3_js_host_launch(text, (unsigned)length));
}

/* bootSave(hex) -> 0 stored, negative on errors; bootSave("") removes the start program */
static JSValue js_boot_save(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JSCStringBuf buffer; const char *text; size_t length;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_BOOT)) return JS_EXCEPTION;
    if (argc < 1) return JS_NewInt32(ctx, -1);
    text = JS_ToCStringLen(ctx, &length, argv[0], &buffer);
    if (!text) return JS_EXCEPTION;
    return JS_NewInt32(ctx, hw3_js_host_boot_save(text, (unsigned)length));
}

/* bootSize() -> code bytes of the start program, 0 = none */
static JSValue js_boot_size(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val; (void)argc; (void)argv;
    if (denied(ctx, HW3_JS_PERM_BOOT)) return JS_EXCEPTION;
    return JS_NewInt32(ctx, hw3_js_host_boot_size());
}

/* keys() -> "0 2 1": key values of the last presses (debug), oldest first; the watch logs them in its key scan */
static JSValue js_keys(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    char text[HW3_JS_KEYS * 4 + 4];
    (void)this_val; (void)argc; (void)argv;
    if (denied(ctx, HW3_JS_PERM_DEBUG)) return JS_EXCEPTION;
    return JS_NewStringLen(ctx, text, hw3_js_host_keys(text, sizeof(text)));
}
static JSValue js_peek(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int address = 0; uint32_t value = 0;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_DEBUG)) return JS_EXCEPTION;
    if (argc < 1 || JS_ToInt32(ctx, &address, argv[0])) return JS_EXCEPTION;
    if (!hw3_js_host_peek((uint32_t)address, &value)) return JS_ThrowRangeError(ctx, "address");
    return JS_NewInt32(ctx, (int)value);
}

/* save(text) -> true when stored */
static JSValue js_save(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JSCStringBuf buffer; const char *text; size_t length;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_STORAGE)) return JS_EXCEPTION;
    if (argc < 1) return JS_FALSE;
    text = JS_ToCStringLen(ctx, &length, argv[0], &buffer);
    if (!text) return JS_EXCEPTION;
    if (length > HW3_JS_STORE) return JS_FALSE;
    return hw3_js_host_save(text, (unsigned)length) ? JS_TRUE : JS_FALSE;
}

/* send(text) -> true when handed to the phone link */
static JSValue js_send(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JSCStringBuf buffer; const char *text; size_t length;
    (void)this_val;
    if (denied(ctx, HW3_JS_PERM_PHONE)) return JS_EXCEPTION;
    if (argc < 1) return JS_FALSE;
    text = JS_ToCStringLen(ctx, &length, argv[0], &buffer);
    if (!text) return JS_EXCEPTION;
    if (length > HW3_JS_MESSAGE) return JS_FALSE;
    return hw3_js_host_send(text, (unsigned)length) ? JS_TRUE : JS_FALSE;
}

/* load() -> saved text or '' */
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    char text[HW3_JS_STORE];
    (void)this_val; (void)argc; (void)argv;
    if (denied(ctx, HW3_JS_PERM_STORAGE)) return JS_EXCEPTION;
    return JS_NewStringLen(ctx, text, hw3_js_host_load(text, sizeof(text)));
}
#include "hw3_stdlib.h"

/* The engine's own checks: stop here instead of running on with a broken state. */
void __assert_func(const char *file, int line, const char *function, const char *expression) {
    (void)file; (void)line; (void)function; (void)expression;
    for (;;) {}
}
void abort(void) { for (;;) {} }
#ifndef HW3_JS_HOST
double fabs(double x) { union { double d; uint64_t u; } v; v.d = x; v.u &= 0x7fffffffffffffffull; return v.d; }
#endif

int hw3_js_selftest(void *memory, unsigned size) {
    /* closures, objects, strings, array methods, floating point: sums to 42 */
    static const char source[] =
        "function mk(n){return function(x){return x*n}}"
        "var o={a:[1,2,3],s:'ab'+'c'};"
        "var t=o.a.map(mk(4)).reduce(function(p,c){return p+c},0);"   /* 24 */
        "t+o.s.length+Math.floor(Math.sqrt(230.5))";                  /* 24 + 3 + 15 */
    JSContext *ctx = JS_NewContext(memory, size, &js_stdlib);
    JSValue value;
    int result = 0;
    if (!ctx) return -1;
    /* a syntax error first: the parser leaves through longjmp and must report an exception */
    value = JS_Eval(ctx, "var x = ;", 9u, "broken", 0);
    if (!JS_IsException(value)) return -4;
    JS_GetException(ctx);
    value = JS_Eval(ctx, source, sizeof(source) - 1u, "selftest", JS_EVAL_RETVAL);
    if (JS_IsException(value)) return -2;
    if (JS_ToInt32(ctx, &result, value)) return -3;
    return result;
}

int hw3_js_open(void *memory, unsigned size, const char *source, unsigned length) {
    JSContext *ctx = JS_NewContext(memory, size, &js_stdlib);
    if (!ctx) return -1;
    if (JS_IsException(JS_Eval(ctx, source, length, "app", 0))) { JS_GetException(ctx); return -2; }
    return 0;
}

int hw3_js_call(void *memory, const char *name, int argc, int a0, int a1) {
    JSContext *ctx = (JSContext *)memory;          /* JS_NewContext places the context at the start of its memory */
    JSValue function = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), name);
    if (!JS_IsFunction(ctx, function)) return 0;
    if (JS_StackCheck(ctx, (uint32_t)argc + 2u)) return -1;
    if (argc > 1) JS_PushArg(ctx, JS_NewInt32(ctx, a1));   /* arguments last to first, then function and this */
    if (argc > 0) JS_PushArg(ctx, JS_NewInt32(ctx, a0));
    JS_PushArg(ctx, function);
    JS_PushArg(ctx, JS_NULL);
    if (JS_IsException(JS_Call(ctx, argc))) { JS_GetException(ctx); return -2; }
    return 1;
}

int hw3_js_call_text(void *memory, const char *name, const char *text, unsigned length) {
    JSContext *ctx = (JSContext *)memory;
    JSValue function = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), name), argument;
    if (!JS_IsFunction(ctx, function)) return 0;
    if (JS_StackCheck(ctx, 3u)) return -1;
    argument = JS_NewStringLen(ctx, text, length);       /* may collect garbage: look the function up again afterwards */
    if (JS_IsException(argument)) { JS_GetException(ctx); return -2; }
    JS_PushArg(ctx, argument);
    JS_PushArg(ctx, JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), name));
    JS_PushArg(ctx, JS_NULL);
    if (JS_IsException(JS_Call(ctx, 1))) { JS_GetException(ctx); return -2; }
    return 1;
}
