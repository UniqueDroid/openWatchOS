/* SPDX-License-Identifier: GPL-2.0-only */
/* Weather: what the phone sent last (city, now, low/high, three days). "Aktualisieren" asks the phone again. */
#include "../ow_app.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>

static const char *const ICON[6] = { OW_ICON_SUN, OW_ICON_CLOUD_SUN, OW_ICON_CLOUD, OW_ICON_RAIN, OW_ICON_SNOW, OW_ICON_STORM };
static const char *const TEXT[6] = { "Sonnig", "Heiter", "Bewölkt", "Regen", "Schnee", "Gewitter" };
static const uint32_t TINT[6] = { 0xFFD60A, 0xFFD60A, 0xC7C7CC, 0x64D2FF, 0xFFFFFF, 0xBF5AF2 };

static void on_refresh(lv_event_t *e) { LV_UNUSED(e); ow_sys_send("weather", "get", 3); }

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    const ow_weather *w = &ow_ui_state()->weather;
    if (!w->valid) {
        lv_obj_t *l = ow_label(scr, &ow_font_20, OW_GREY, "Noch keine Wetterdaten vom Handy");
        lv_obj_center(l);
        lv_obj_t *b = ow_button(scr, 80, OW_BLUE, OW_ICON_SYNC, on_refresh, 0);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -40);
        return;
    }
    uint8_t c = w->code < 6 ? w->code : 2;
    lv_obj_t *city = ow_label(scr, &ow_font_28, 0xFFFFFF, w->city);
    lv_obj_align(city, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_t *ic = ow_label(scr, &ow_icon_56, TINT[c], ICON[c]);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 56, 82);
    lv_obj_t *t = ow_label(scr, &ow_font_64, 0xFFFFFF, "");
    lv_label_set_text_fmt(t, "%d°", w->temp);
    lv_obj_align(t, LV_ALIGN_TOP_RIGHT, -54, 72);
    char s[48];
    snprintf(s, sizeof s, "%s · %d° / %d°", TEXT[c], w->low, w->high);
    lv_obj_t *d = ow_label(scr, &ow_font_20, OW_GREY, s);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 160);
    static const char *const DAY[3] = { "Morgen", "Übermorgen", "In 3 Tagen" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, OW_W - 32, 66);
        lv_obj_set_pos(row, 16, 200 + i * 74);
        lv_obj_set_style_radius(row, 33, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(OW_CARD), 0);
        lv_obj_set_scrollable(row, false);
        uint8_t k = w->days[i].code < 6 ? w->days[i].code : 2;
        lv_obj_t *n = ow_label(row, &ow_font_20, 0xFFFFFF, DAY[i]);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 22, 0);
        lv_obj_t *di = ow_label(row, &ow_font_28, TINT[k], ICON[k]);
        lv_obj_align(di, LV_ALIGN_CENTER, 40, 0);
        lv_obj_t *v = ow_label(row, &ow_font_20, OW_GREY, "");
        lv_label_set_text_fmt(v, "%d° / %d°", w->days[i].low, w->days[i].high);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, -22, 0);
    }
}

const ow_app ow_app_weather = { "weather", "Wetter", OW_ICON_CLOUD_SUN, OW_BLUE, open, 0, 0 };
