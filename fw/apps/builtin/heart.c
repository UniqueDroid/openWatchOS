/* SPDX-License-Identifier: GPL-2.0-only */
/* Pulse: current value, and the day as 24 hourly bars with lowest and highest value. */
#include "../ow_app.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    const ow_state *s = ow_ui_state();
    lv_obj_t *ic = ow_label(scr, &ow_icon_56, OW_RED, OW_ICON_HEART);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 64, 46);
    lv_obj_t *v = ow_label(scr, &ow_font_64, 0xFFFFFF, "");
    lv_label_set_text_fmt(v, "%lu", (unsigned long)s->v.heart);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, 150, 32);
    lv_obj_t *u = ow_label(scr, &ow_font_20, OW_GREY, "Schläge pro Minute");
    lv_obj_align(u, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_t *ch = lv_chart_create(scr);
    lv_obj_set_size(ch, OW_W - 40, 170);
    lv_obj_align(ch, LV_ALIGN_TOP_MID, 0, 160);
    lv_chart_set_type(ch, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(ch, 24);
    lv_chart_set_axis_range(ch, LV_CHART_AXIS_PRIMARY_Y, 40, 160);
    lv_chart_set_div_line_count(ch, 3, 0);
    lv_obj_set_style_bg_color(ch, lv_color_hex(OW_CARD), 0);
    lv_obj_set_style_border_width(ch, 0, 0);
    lv_obj_set_style_radius(ch, 24, 0);
    lv_obj_set_style_line_color(ch, lv_color_hex(0x2C2C2E), 0);
    lv_obj_set_style_pad_column(ch, 2, 0);
    lv_obj_set_style_pad_all(ch, 14, 0);
    lv_chart_series_t *ser = lv_chart_add_series(ch, lv_color_hex(OW_RED), LV_CHART_AXIS_PRIMARY_Y);
    int lo = 999, hi = 0;
    for (int i = 0; i < 24; i++) {
        int x = s->heart_hours[i];
        lv_chart_set_next_value(ch, ser, x ? x : LV_CHART_POINT_NONE);
        if (x && x < lo) lo = x;
        if (x > hi) hi = x;
    }
    lv_obj_t *r = ow_label(scr, &ow_font_20, 0xFFFFFF, "");
    if (hi) lv_label_set_text_fmt(r, "Heute  %d – %d", lo, hi);
    else lv_label_set_text(r, "Heute noch keine Messung");
    lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 346);
    lv_obj_t *h = ow_label(scr, &ow_font_20, OW_GREY, "0 Uhr                 12                 24");
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 384);
}

const ow_app ow_app_heart = { "heart", "Puls", OW_ICON_HEART, OW_RED, open, 0, 0 };
