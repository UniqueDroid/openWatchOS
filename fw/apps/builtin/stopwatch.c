/* SPDX-License-Identifier: GPL-2.0-only */
/* Stopwatch: start/stop, laps (up to 20), reset. Keeps running in the background while other pages show. */
#include "../ow_app.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>

static struct { uint8_t running; uint32_t start, acc, laps[20]; uint8_t nlaps; } sw;
static lv_obj_t *time_l, *go, *lap, *list;

static uint32_t elapsed(void) { return sw.acc + (sw.running ? ow_sys_ms() - sw.start : 0u); }

static void fmt(char *out, size_t cap, uint32_t ms) {
    snprintf(out, cap, "%02lu:%02lu.%02lu", (unsigned long)(ms / 60000u % 100u), (unsigned long)(ms / 1000u % 60u), (unsigned long)(ms / 10u % 100u));
}

static void refresh(void) {
    char t[16];
    fmt(t, sizeof t, elapsed());
    lv_label_set_text(time_l, t);
    lv_obj_set_style_bg_color(go, lv_color_hex(sw.running ? OW_RED : OW_GREEN), 0);
    lv_label_set_text(lv_obj_get_child(go, 0), sw.running ? OW_ICON_PAUSE : OW_ICON_PLAY);
    lv_label_set_text(lv_obj_get_child(lap, 0), sw.running || !elapsed() ? OW_ICON_FLAG : OW_ICON_REDO);
}

static void laps(void) {
    lv_obj_clean(list);
    for (int i = sw.nlaps - 1; i >= 0 && i >= sw.nlaps - 3; i--) {
        char t[40], d[16];
        fmt(d, sizeof d, sw.laps[i] - (i ? sw.laps[i - 1] : 0u));
        snprintf(t, sizeof t, "Runde %d    %s", i + 1, d);
        ow_label(list, &ow_font_20, i == sw.nlaps - 1 ? 0xFFFFFF : OW_GREY, t);
    }
}

static void on_go(lv_event_t *e) {
    LV_UNUSED(e);
    if (sw.running) { sw.acc += ow_sys_ms() - sw.start; sw.running = 0; }
    else { sw.start = ow_sys_ms(); sw.running = 1; }
    refresh();
}

static void on_lap(lv_event_t *e) {
    LV_UNUSED(e);
    if (sw.running) { if (sw.nlaps < 20) sw.laps[sw.nlaps++] = elapsed(); }
    else { sw.acc = 0; sw.nlaps = 0; }
    laps(); refresh();
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *t = ow_label(scr, &ow_font_28, OW_ORANGE, "Stoppuhr");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
    time_l = ow_label(scr, &ow_font_64, 0xFFFFFF, "");
    lv_obj_align(time_l, LV_ALIGN_TOP_MID, 0, 92);
    list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, OW_W, 96);
    lv_obj_set_y(list, 182);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lap = ow_button(scr, 104, 0x3A3A3C, OW_ICON_FLAG, on_lap, 0);
    lv_obj_align(lap, LV_ALIGN_BOTTOM_LEFT, 54, -36);
    go = ow_button(scr, 104, OW_GREEN, OW_ICON_PLAY, on_go, 0);
    lv_obj_align(go, LV_ALIGN_BOTTOM_RIGHT, -54, -36);
    laps(); refresh();
}

static void tick(void) { if (sw.running) refresh(); }

const ow_app ow_app_stopwatch = { "stopwatch", "Stoppuhr", OW_ICON_STOPWATCH, OW_ORANGE, open, 0, tick };
