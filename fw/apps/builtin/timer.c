/* SPDX-License-Identifier: GPL-2.0-only */
/* Timer: six presets, a ring that runs down, pause/continue and cancel; vibrates at the end. */
#include "../ow_app.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>

static const uint8_t PRESET[6] = { 1, 3, 5, 10, 15, 30 };
static struct { uint32_t total, end, left; uint8_t state; } tm;   /* state: 0 choose, 1 running, 2 paused, 3 done */
static lv_obj_t *page, *arc, *rest, *pause_b;

static void build(lv_obj_t *scr);

static void on_preset(lv_event_t *e) {
    tm.total = PRESET[(intptr_t)lv_event_get_user_data(e)] * 60000u;
    tm.end = ow_sys_ms() + tm.total; tm.state = 1;
    build(lv_obj_get_parent(page));
}

static void on_pause(lv_event_t *e) {
    LV_UNUSED(e);
    if (tm.state == 1) { tm.left = tm.end - ow_sys_ms(); tm.state = 2; }
    else if (tm.state == 2) { tm.end = ow_sys_ms() + tm.left; tm.state = 1; }
    lv_label_set_text(lv_obj_get_child(pause_b, 0), tm.state == 1 ? OW_ICON_PAUSE : OW_ICON_PLAY);
}

static void on_cancel(lv_event_t *e) { LV_UNUSED(e); tm.state = 0; build(lv_obj_get_parent(page)); }

static uint32_t left(void) {
    if (tm.state == 2) return tm.left;
    if (tm.state != 1) return 0;
    int32_t l = (int32_t)(tm.end - ow_sys_ms());
    return l > 0 ? (uint32_t)l : 0u;
}

static void show(void) {
    if (!rest) return;
    uint32_t l = left(), s = (l + 999u) / 1000u;
    lv_label_set_text_fmt(rest, "%02lu:%02lu", (unsigned long)(s / 60u), (unsigned long)(s % 60u));
    lv_arc_set_value(arc, tm.total ? (int32_t)(l * 1000u / tm.total) : 0);
}

static void build(lv_obj_t *scr) {
    if (page) lv_obj_delete_async(page);   /* we may be inside an event of one of its buttons */
    rest = 0;
    page = lv_obj_create(scr);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, OW_W, OW_H);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);   /* covers the old page until it is gone */
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_scrollable(page, false);
    lv_obj_t *t = ow_label(page, &ow_font_28, OW_PURPLE, "Timer");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
    if (tm.state == 0) {
        for (int i = 0; i < 6; i++) {
            lv_obj_t *b = ow_button(page, 104, 0x2C2C2E, "", on_preset, (void *)(intptr_t)i);
            lv_obj_set_pos(b, 22 + (i % 3) * 112, 90 + (i / 3) * 150);
            lv_obj_t *n = lv_obj_get_child(b, 0);
            lv_obj_set_style_text_font(n, &ow_font_40, 0);
            lv_label_set_text_fmt(n, "%u", PRESET[i]);
            lv_obj_t *m = ow_label(page, &ow_font_20, OW_GREY, "min");
            lv_obj_set_pos(m, 22 + (i % 3) * 112 + 34, 90 + (i / 3) * 150 + 112);
        }
        return;
    }
    arc = lv_arc_create(page);
    lv_obj_set_size(arc, 260, 260);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 66);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_range(arc, 0, 1000);
    lv_obj_remove_style(arc, 0, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_style_arc_width(arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(OW_PURPLE), LV_PART_INDICATOR);
    rest = ow_label(page, &ow_font_64, 0xFFFFFF, "");
    lv_obj_align(rest, LV_ALIGN_TOP_MID, 0, 158);
    lv_obj_t *c = ow_button(page, 88, 0x3A3A3C, LV_SYMBOL_CLOSE, on_cancel, 0);
    lv_obj_align(c, LV_ALIGN_BOTTOM_LEFT, 70, -20);
    pause_b = ow_button(page, 88, OW_PURPLE, tm.state == 1 ? OW_ICON_PAUSE : OW_ICON_PLAY, on_pause, 0);
    lv_obj_align(pause_b, LV_ALIGN_BOTTOM_RIGHT, -70, -20);
    if (tm.state == 3) { lv_label_set_text(rest, "00:00"); lv_obj_t *d = ow_label(page, &ow_font_20, OW_PURPLE, "Zeit abgelaufen"); lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 240); }
    show();
}

static void open(lv_obj_t *scr, const char *arg) { LV_UNUSED(arg); page = 0; build(scr); }
static void close(void) { page = 0; rest = 0; }

static void tick(void) {
    if (tm.state == 1 && !left()) { tm.state = 3; ow_sys_vibrate(800); build(lv_obj_get_parent(page)); return; }
    show();
}

const ow_app ow_app_timer = { "timer", "Timer", OW_ICON_HOURGLASS, OW_PURPLE, open, close, tick };
