/* SPDX-License-Identifier: GPL-2.0-only */
/* Settings: brightness, always-on, do not disturb, vibration, 12-hour clock, raise to wake, watch face, language,
 * about. Every change is saved at once (sys/settings.txt). */
#include "../ow_app.h"
#include "../ow_pkg.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>
#include <string.h>

enum { S_AOD, S_DND, S_VIB, S_H12, S_RAISE };

static uint8_t *field(int which) {
    ow_settings *s = ow_settings_get();
    switch (which) {
    case S_AOD: return &s->aod;
    case S_DND: return &s->dnd;
    case S_VIB: return &s->vibration;
    case S_H12: return &s->h12;
    default: return &s->raise_to_wake;
    }
}

static void on_switch(lv_event_t *e) {
    *field((int)(intptr_t)lv_event_get_user_data(e)) = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    ow_settings_save();
    if (ow_ui_state()) ow_ui_update();
}

static void on_bright(lv_event_t *e) {
    ow_settings_get()->brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    ow_sys_brightness(ow_settings_get()->brightness);
    ow_settings_save();
}

static void switch_row(lv_obj_t *box, const char *sym, uint32_t colour, const char *text, int which) {
    lv_obj_t *row = ow_row(box, sym, colour, text, 0);
    lv_obj_set_width(lv_obj_get_child(row, 1), 150);
    lv_obj_t *s = ow_switch(row, *field(which), on_switch, (void *)(intptr_t)which);
    lv_obj_align(s, LV_ALIGN_RIGHT_MID, -6, 0);
}

static void on_lang(lv_event_t *e) {
    ow_settings *s = ow_settings_get();
    s->lang ^= 1;
    ow_settings_save();
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 2), s->lang ? "English" : "Deutsch");
}

static void on_faces(lv_event_t *e) { LV_UNUSED(e); ow_app_open(ow_app_find("faces"), 0); }

/* ---- about ---- */

static void count(const char *n, int d, void *u) { LV_UNUSED(n); LV_UNUSED(d); (*(int *)u)++; }

static void open_about(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *box = ow_page(scr, "Über", 0xFFFFFF);
    static ow_pkg_info pk[OW_PKG_MAX];
    int apps = ow_pkg_list(pk, OW_PKG_MAX), faces = 0;
    ow_sys_list("faces", count, &faces);
    char t[64];
    lv_obj_t *logo = ow_round_icon(box, 110, OW_BLUE, OW_ICON_GLOBE);
    LV_UNUSED(logo);
    ow_label(box, &ow_font_28, 0xFFFFFF, "openWatchOS " OW_VERSION);
    ow_label(box, &ow_font_20, OW_GREY, "LVGL 9.6 · M78 (JL701N)");
    snprintf(t, sizeof t, "%d JS-Apps · %d Zifferblätter", apps, faces);
    ow_label(box, &ow_font_20, OW_GREY, t);
    snprintf(t, sizeof t, "Akku %u %%", ow_sys_battery());
    ow_label(box, &ow_font_20, OW_GREY, t);
    ow_label(box, &ow_font_20, OW_GREY, "GPL-2.0 · github.com/UniqueDroid/openWatchOS");
}
static const ow_app about = { "about", "Über", OW_ICON_INFO, 0x8E8E93, open_about, 0, 0 };
static void on_about(lv_event_t *e) { LV_UNUSED(e); ow_app_open(&about, 0); }

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *box = ow_page(scr, "Einstellungen", 0xFFFFFF);
    /* brightness: a slider in a card */
    lv_obj_t *card = ow_row(box, OW_ICON_SUN, OW_ORANGE, "", 0);
    lv_obj_t *sl = lv_slider_create(card);
    lv_obj_set_size(sl, 200, 14);
    lv_obj_align(sl, LV_ALIGN_LEFT_MID, 84, 0);
    lv_slider_set_range(sl, 5, 100);
    lv_slider_set_value(sl, ow_settings_get()->brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x39393D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(OW_ORANGE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_white(), LV_PART_KNOB);
    lv_obj_add_event_cb(sl, on_bright, LV_EVENT_VALUE_CHANGED, 0);
    switch_row(box, LV_SYMBOL_EYE_OPEN, OW_GREEN, "Immer an", S_AOD);
    switch_row(box, OW_ICON_MOON, 0x5E5CE6, "Ruhe", S_DND);
    switch_row(box, LV_SYMBOL_BELL, OW_RED, "Vibration", S_VIB);
    switch_row(box, OW_ICON_ALARM, OW_YELLOW, "12 Stunden", S_H12);
    switch_row(box, LV_SYMBOL_UP, OW_BLUE, "Anheben", S_RAISE);
    lv_obj_t *f = ow_row(box, OW_ICON_PALETTE, 0xAC8E68, "Zifferblatt", ow_settings_get()->face);
    lv_obj_add_event_cb(f, on_faces, LV_EVENT_SHORT_CLICKED, 0);
    lv_obj_t *l = ow_row(box, OW_ICON_GLOBE, 0x64D2FF, "Sprache", ow_settings_get()->lang ? "English" : "Deutsch");
    lv_obj_add_event_cb(l, on_lang, LV_EVENT_SHORT_CLICKED, 0);
    lv_obj_t *a = ow_row(box, OW_ICON_INFO, 0x8E8E93, "Über", OW_VERSION);
    lv_obj_add_event_cb(a, on_about, LV_EVENT_SHORT_CLICKED, 0);
}

const ow_app ow_app_settings = { "settings", "Einstellungen", OW_ICON_COG, 0x8E8E93, open, 0, 0 };
