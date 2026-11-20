/* SPDX-License-Identifier: GPL-2.0-only */
/* Alarms: up to 6, each with time and on/off, kept in sys/alarms.txt ("HH:MM 1" per line); a new alarm is set with
 * two rollers. The firmware's main loop checks them once a minute (ow_alarm_due). */
#include "../ow_app.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX 6
static struct { uint8_t hour, minute, on; } al[MAX];
static int n;
static lv_obj_t *box, *roll_h, *roll_m;

static void load(void) {
    char buf[256];
    int len = ow_sys_read("sys/alarms.txt", buf, sizeof buf - 1);
    n = 0;
    if (len <= 0) { al[0].hour = 6; al[0].minute = 45; al[0].on = 1; al[1].hour = 8; al[1].minute = 0; al[1].on = 0; n = 2; return; }
    buf[len] = 0;
    for (char *p = buf; *p && n < MAX;) {
        unsigned h, m, o;
        if (sscanf(p, "%u:%u %u", &h, &m, &o) == 3 && h < 24 && m < 60) { al[n].hour = (uint8_t)h; al[n].minute = (uint8_t)m; al[n].on = o != 0; n++; }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

static void save(void) {
    char buf[256]; int k = 0;
    for (int i = 0; i < n; i++) k += snprintf(buf + k, sizeof buf - (size_t)k, "%02u:%02u %u\n", al[i].hour, al[i].minute, al[i].on);
    ow_sys_write("sys/alarms.txt", buf, (unsigned)k);
}

int ow_alarm_due(uint8_t hour, uint8_t minute) {
    load();
    for (int i = 0; i < n; i++) if (al[i].on && al[i].hour == hour && al[i].minute == minute) return 1;
    return 0;
}

static void fill(void);

static void on_switch(lv_event_t *e) {
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    al[i].on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    save();
}

static void on_save(lv_event_t *e) {
    LV_UNUSED(e);
    if (n < MAX) { al[n].hour = (uint8_t)lv_roller_get_selected(roll_h); al[n].minute = (uint8_t)lv_roller_get_selected(roll_m); al[n].on = 1; n++; save(); }
    ow_app_back();
}

static lv_obj_t *roller(lv_obj_t *p, int count, int sel) {
    static char opts[2][200];
    char *o = opts[count == 24 ? 0 : 1]; int k = 0;
    for (int i = 0; i < count; i++) k += snprintf(o + k, 200 - (size_t)k, i ? "\n%02d" : "%02d", i);
    lv_obj_t *r = lv_roller_create(p);
    lv_roller_set_options(r, o, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(r, 3);
    lv_roller_set_selected(r, (uint32_t)sel, LV_ANIM_OFF);
    lv_obj_set_width(r, 120);
    lv_obj_set_style_text_font(r, &ow_font_40, 0);
    lv_obj_set_style_bg_color(r, lv_color_black(), 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_text_color(r, lv_color_hex(OW_GREY), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x2C2C2E), LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_radius(r, 20, LV_PART_SELECTED);
    return r;
}

static void open_new(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *t = ow_label(scr, &ow_font_28, OW_YELLOW, "Neuer Wecker");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
    roll_h = roller(scr, 24, 7);
    lv_obj_align(roll_h, LV_ALIGN_TOP_MID, -70, 80);
    roll_m = roller(scr, 60, 30);
    lv_obj_align(roll_m, LV_ALIGN_TOP_MID, 70, 80);
    lv_obj_t *c = ow_label(scr, &ow_font_40, 0xFFFFFF, ":");
    lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_t *b = ow_button(scr, 96, OW_GREEN, OW_ICON_CHECK, on_save, 0);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -28);
}
static const ow_app alarm_new = { "alarm.new", "Neuer Wecker", OW_ICON_ALARM, OW_YELLOW, open_new, 0, 0 };

static void on_add(lv_event_t *e) { LV_UNUSED(e); ow_app_open(&alarm_new, 0); }

static void fill(void) {
    lv_obj_clean(box);
    for (int i = 0; i < n; i++) {
        char t[8]; snprintf(t, sizeof t, "%02u:%02u", al[i].hour, al[i].minute);
        lv_obj_t *row = ow_row(box, 0, 0, "", 0);
        lv_obj_t *l = lv_obj_get_child(row, 0);
        lv_obj_set_style_text_font(l, &ow_font_40, 0);
        lv_obj_set_height(l, LV_SIZE_CONTENT);
        lv_label_set_text(l, t);
        lv_obj_set_style_text_color(l, lv_color_hex(al[i].on ? 0xFFFFFF : OW_GREY), 0);
        lv_obj_t *s = ow_switch(row, al[i].on, on_switch, (void *)(intptr_t)i);
        lv_obj_align(s, LV_ALIGN_RIGHT_MID, -6, 0);
    }
    if (n < MAX) {
        lv_obj_t *row = ow_row(box, OW_ICON_PLUS, OW_YELLOW, "Wecker", 0);
        lv_obj_add_event_cb(row, on_add, LV_EVENT_SHORT_CLICKED, 0);
    }
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    load();
    box = ow_page(scr, "Wecker", OW_YELLOW);
    fill();
}
static void tick(void) { static int was = -1; if (was != n) { was = n; if (box) fill(); } }

const ow_app ow_app_alarm = { "alarm", "Wecker", OW_ICON_ALARM, OW_YELLOW, open, 0, tick };
