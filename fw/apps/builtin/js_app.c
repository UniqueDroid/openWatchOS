/* SPDX-License-Identifier: GPL-2.0-only */
/* JS apps in the own firmware: the same scripts as on the patched stock firmware (watch.txt of an installed
 * .oapps package, fw/js), drawn into an LVGL canvas of the whole screen. The platform functions of fw/js/hw3_js.h
 * are implemented here; draw() runs after open, every tap and every timer tick. */
#include "../ow_app.h"
#include "../ow_pkg.h"
#include "../../js/hw3_js.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JS_MEMORY (64u * 1024u)
#define SCRIPT_MAX (96u * 1024u)

static struct {
    char id[48];
    uint32_t permissions, period, next;
    void *memory;
    lv_draw_buf_t *buf;
    lv_obj_t *canvas, *err;
    int failed;
} js;

/* ---- platform functions for the JS layer ---- */

void hw3_js_host_rect(int x1, int y1, int x2, int y2, uint32_t rgb) {
    if (!js.buf) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= OW_W) x2 = OW_W - 1;
    if (y2 >= OW_H) y2 = OW_H - 1;
    uint16_t c = (uint16_t)(((rgb >> 19) & 31u) << 11 | ((rgb >> 10) & 63u) << 5 | ((rgb >> 3) & 31u));
    for (int y = y1; y <= y2; y++) {
        uint16_t *p = (uint16_t *)(js.buf->data + (size_t)y * js.buf->header.stride) + x1;
        for (int x = x1; x <= x2; x++) *p++ = c;
    }
}

void hw3_js_host_text(int x, int y, const uint16_t *text) {
    if (!js.canvas) return;
    char utf8[HW3_JS_TEXT * 3 + 1]; int n = 0;
    for (int i = 0; text[i] && n < (int)sizeof utf8 - 4; i++) {
        unsigned c = text[i];
        if (c < 0x80) utf8[n++] = (char)c;
        else if (c < 0x800) { utf8[n++] = (char)(0xC0 | c >> 6); utf8[n++] = (char)(0x80 | (c & 63)); }
        else { utf8[n++] = (char)(0xE0 | c >> 12); utf8[n++] = (char)(0x80 | (c >> 6 & 63)); utf8[n++] = (char)(0x80 | (c & 63)); }
    }
    utf8[n] = 0;
    /* as on the stock firmware: white, centred on x, y on the baseline */
    const lv_font_t *f = &ow_font_28;
    lv_layer_t layer;
    lv_canvas_init_layer(js.canvas, &layer);
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = f; d.color = lv_color_white(); d.text = utf8; d.align = LV_TEXT_ALIGN_CENTER;
    int top = y - (f->line_height - f->base_line);
    lv_area_t a = { x - 180, top, x + 180, top + f->line_height };
    lv_draw_label(&layer, &d, &a);
    lv_canvas_finish_layer(js.canvas, &layer);
}

int hw3_js_host_allowed(unsigned p) { return (js.permissions >> p) & 1u; }
uint32_t hw3_js_host_seconds(void) { return ow_sys_seconds(); }
void hw3_js_host_timer(unsigned ms) { js.period = ms; js.next = ow_sys_ms() + ms; }
void hw3_js_host_vibrate(unsigned ms) { ow_sys_vibrate(ms); }

int hw3_js_host_save(const char *text, unsigned len) {
    char p[96]; snprintf(p, sizeof p, "apps/%s/data.txt", js.id);
    return len <= HW3_JS_STORE && !ow_sys_write(p, text, len);
}

unsigned hw3_js_host_load(char *out, unsigned cap) {
    char p[96]; snprintf(p, sizeof p, "apps/%s/data.txt", js.id);
    int n = ow_sys_read(p, out, cap);
    return n > 0 ? (unsigned)n : 0u;
}

int hw3_js_host_send(const char *text, unsigned len) {
    char ch[64]; snprintf(ch, sizeof ch, "app:%s", js.id);
    return len <= HW3_JS_MESSAGE && ow_sys_send(ch, text, len);
}

unsigned hw3_js_host_keys(char *out, unsigned cap) { if (cap) out[0] = 0; return 0; }
int hw3_js_host_peek(uint32_t address, uint32_t *value) { (void)address; *value = 0; return 0; }   /* no stock RAM here */
int hw3_js_host_noise(int restart) { (void)restart; return -2; }                                  /* not yet */
int hw3_js_host_launch(const char *hex, unsigned length) { (void)hex; (void)length; return -1; }  /* OWLP: stock firmware only */
int hw3_js_host_boot_save(const char *hex, unsigned length) { (void)hex; (void)length; return -1; }
int hw3_js_host_boot_size(void) { return 0; }

/* ---- the app page ---- */

static void redraw(void) {
    if (js.failed) return;
    int r = hw3_js_call(js.memory, "draw", 0, 0, 0);
    if (r < 0) { js.failed = 1; lv_label_set_text(js.err, "Fehler in draw()"); }
    lv_obj_invalidate(js.canvas);
}

static void on_tap(lv_event_t *e) {
    LV_UNUSED(e);
    if (js.failed) return;
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    hw3_js_call(js.memory, "tap", 2, p.x, p.y);
    redraw();
}

/* text from the app's companion script on the phone (the platform calls this for channel "app:<id>") */
void ow_js_message(const char *id, const char *text, unsigned len) {
    if (!js.memory || strcmp(id, js.id)) return;
    hw3_js_call_text(js.memory, "message", text, len);
    redraw();
}

static void open(lv_obj_t *scr, const char *id) {
    memset(&js, 0, sizeof js);
    snprintf(js.id, sizeof js.id, "%s", id ? id : "");
    ow_pkg_info info;
    if (!ow_pkg_get(js.id, &info)) js.permissions = info.permissions;
    js.buf = lv_draw_buf_create(OW_W, OW_H, LV_COLOR_FORMAT_RGB565, OW_W * 2);
    js.canvas = lv_canvas_create(scr);
    lv_canvas_set_draw_buf(js.canvas, js.buf);
    lv_canvas_fill_bg(js.canvas, lv_color_black(), LV_OPA_COVER);
    lv_obj_set_clickable(js.canvas, true);
    lv_obj_add_event_cb(js.canvas, on_tap, LV_EVENT_SHORT_CLICKED, 0);
    js.err = ow_label(scr, &ow_font_20, OW_RED, "");
    lv_obj_align(js.err, LV_ALIGN_BOTTOM_MID, 0, -20);
    char path[96];
    snprintf(path, sizeof path, "apps/%s/watch.txt", js.id);
    int n = ow_sys_size(path);
    char *src = n > 0 && (unsigned)n < SCRIPT_MAX ? malloc((size_t)n + 1) : 0;
    js.memory = malloc(JS_MEMORY);
    if (!src || !js.memory || ow_sys_read(path, src, (unsigned)n) != n) {
        js.failed = 1; lv_label_set_text(js.err, "App nicht gefunden"); free(src); return;
    }
    src[n] = 0;
    char *s = src; unsigned len = (unsigned)n;
    if (len >= 3 && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF) { s += 3; len -= 3; }   /* BOM */
    int r = hw3_js_open(js.memory, JS_MEMORY, s, len);
    free(src);   /* the engine keeps what it needs in its memory */
    if (r) { js.failed = 1; lv_label_set_text(js.err, r == -2 ? "Fehler im Skript" : "Kein Speicher"); return; }
    redraw();
}

static void close(void) {
    free(js.memory);
    if (js.buf) lv_draw_buf_destroy(js.buf);
    memset(&js, 0, sizeof js);
}

static void tick(void) {
    if (!js.period || js.failed) return;
    if ((int32_t)(ow_sys_ms() - js.next) < 0) return;
    js.next += js.period;
    hw3_js_call(js.memory, "tick", 0, 0, 0);
    redraw();
}

const ow_app ow_app_js = { "js", "JS-App", OW_ICON_CODE, 0x32ADE6, open, close, tick };
