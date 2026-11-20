/* SPDX-License-Identifier: GPL-2.0-only */
#include "ow_widgets.h"

static lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

lv_obj_t *ow_label(lv_obj_t *parent, const lv_font_t *font, uint32_t colour, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, hex(colour), 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *ow_round_icon(lv_obj_t *parent, int size, uint32_t colour, const char *symbol) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, hex(colour), 0);
    lv_obj_set_style_bg_grad_color(c, lv_color_darken(hex(colour), 60), 0);
    lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_t *s = ow_label(c, size >= 90 ? &ow_icon_56 : &ow_font_28, 0xFFFFFF, symbol);
    lv_obj_center(s);
    return c;
}

lv_obj_t *ow_page(lv_obj_t *screen, const char *title, uint32_t title_colour) {
    lv_obj_t *t = ow_label(screen, &ow_font_28, title_colour, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_t *box = lv_obj_create(screen);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, OW_W, OW_H - 66);
    lv_obj_set_y(box, 66);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 10, 0);
    lv_obj_set_style_pad_bottom(box, 40, 0);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    return box;
}

lv_obj_t *ow_row(lv_obj_t *list, const char *symbol, uint32_t colour, const char *text, const char *right) {
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, OW_W - 32, 76);
    lv_obj_set_style_radius(row, 38, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, hex(OW_CARD), 0);
    lv_obj_set_style_bg_color(row, hex(OW_CARD_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(row, 10, 0);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, true);
    int x = 14;
    if (symbol) {
        lv_obj_t *ic = ow_round_icon(row, 56, colour, symbol);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);
        x = 72;
    }
    lv_obj_t *l = ow_label(row, &ow_font_28, 0xFFFFFF, text);
    lv_obj_set_size(l, right ? 160 : OW_W - 32 - x - 24, lv_font_get_line_height(&ow_font_28));   /* one line, dots */
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, x, 0);
    if (right) {
        lv_obj_t *r = ow_label(row, &ow_font_20, OW_GREY, right);
        lv_obj_align(r, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    return row;
}

lv_obj_t *ow_button(lv_obj_t *parent, int size, uint32_t colour, const char *symbol, lv_event_cb_t cb, void *user) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, size, size);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, hex(colour), 0);
    lv_obj_set_style_bg_color(b, lv_color_lighten(hex(colour), 40), LV_STATE_PRESSED);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, true);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *s = ow_label(b, size >= 110 ? &ow_icon_56 : &ow_font_28, 0xFFFFFF, symbol);
    lv_obj_center(s);
    return b;
}

lv_obj_t *ow_switch(lv_obj_t *parent, int on, lv_event_cb_t cb, void *user) {
    lv_obj_t *s = lv_switch_create(parent);
    lv_obj_set_size(s, 72, 40);
    lv_obj_set_style_bg_color(s, hex(0x39393D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, hex(OW_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_KNOB);
    if (on) lv_obj_add_state(s, LV_STATE_CHECKED);
    if (cb) lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, user);
    return s;
}
