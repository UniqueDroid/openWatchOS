/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS user interface, see ow_ui.h. Dark, round, large touch targets; all colours on black (AMOLED). */
#include "ow_ui.h"
#include <stdio.h>

LV_FONT_DECLARE(ow_font_20)
LV_FONT_DECLARE(ow_font_28)
LV_FONT_DECLARE(ow_font_64)

#define W 368
#define H 448

static ow_state *st;
static lv_obj_t *tv, *tiles[5], *face, *ctl_time, *ctl_date, *ctl_btn[6], *act_arc[3], *act_val[3], *note_box;

static lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

/* ---- watch face: the owf engine draws into the draw buffer ---- */

static void face_draw(lv_event_t *e) {
    lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (!st || !st->face) return;
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const lv_area_t *buf = dc->buf_area, *clip = dc->clip_area;
    /* the engine draws whole screen rows: only when the face sits at x 0 with full width and the buffer is full width */
    if (a.x1 != 0 || lv_area_get_width(&a) != OWF_W || buf->x1 != 0 || lv_area_get_width(buf) != OWF_W) return;
    int y0 = clip->y1 > a.y1 ? clip->y1 : a.y1, y1 = clip->y2 < a.y2 ? clip->y2 : a.y2;
    if (y0 > y1) return;
    lv_color_t *px = (lv_color_t *)dc->buf + (y0 - buf->y1) * OWF_W;
    /* screen row y of the face = y - a.y1 (the face moves with the tile while swiping) */
    owf_band b = { (uint16_t *)px, y0 - a.y1, y1 - y0 + 1, LV_COLOR_16_SWAP };
    for (int i = 0; i < b.rows * OWF_W; i++) px[i] = lv_color_black();
    owf_render(st->face, st->face_len, &st->v, &b, 0);
}

static lv_obj_t *face_create(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, W, H);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(o, face_draw, LV_EVENT_DRAW_MAIN, 0);
    return o;
}

/* ---- small helpers ---- */

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t colour, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, hex(colour), 0);
    lv_label_set_text(l, text);
    return l;
}

static lv_obj_t *round_icon(lv_obj_t *parent, int size, uint32_t colour, const char *symbol) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, hex(colour), 0);
    lv_obj_set_style_bg_grad_color(c, lv_color_darken(hex(colour), 60), 0);
    lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *s = label(c, &ow_font_28, 0xFFFFFF, symbol);
    lv_obj_center(s);
    return c;
}

/* ---- apps ---- */

typedef struct app { const char *name, *symbol; uint32_t colour; } app;
static const app APPS[] = {
    { "Aktivität", LV_SYMBOL_SHUFFLE, 0x30D158 }, { "Puls", LV_SYMBOL_CHARGE, 0xFF375F },
    { "Schlaf", LV_SYMBOL_EYE_CLOSE, 0x5E5CE6 },   { "Training", LV_SYMBOL_PLAY, 0xFF9F0A },
    { "Wetter", LV_SYMBOL_IMAGE, 0x0A84FF },       { "Musik", LV_SYMBOL_AUDIO, 0xFF2D55 },
    { "Wecker", LV_SYMBOL_BELL, 0xFFD60A },        { "Stoppuhr", LV_SYMBOL_LOOP, 0xFF9500 },
    { "Timer", LV_SYMBOL_REFRESH, 0xBF5AF2 },      { "Kompass", LV_SYMBOL_GPS, 0x64D2FF },
    { "Zifferblätter", LV_SYMBOL_IMAGE, 0xAC8E68 }, { "JS-Apps", LV_SYMBOL_LIST, 0x32ADE6 },
    { "Einstellungen", LV_SYMBOL_SETTINGS, 0x8E8E93 },
};

static void apps_create(lv_obj_t *t) {
    lv_obj_t *list = lv_obj_create(t);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, W, H);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(list, 70, 0);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    for (unsigned i = 0; i < sizeof APPS / sizeof APPS[0]; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, W - 32, 76);
        lv_obj_set_style_radius(row, 38, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, hex(0x1C1C1E), 0);
        lv_obj_set_style_bg_color(row, hex(0x2C2C2E), LV_STATE_PRESSED);
        lv_obj_set_style_pad_left(row, 10, 0);
        lv_obj_set_style_translate_x(row, 16, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *ic = round_icon(row, 58, APPS[i].colour, APPS[i].symbol);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *l = label(row, &ow_font_28, 0xFFFFFF, APPS[i].name);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 78, 0);
    }
}

/* ---- control centre ---- */

static const struct { const char *symbol, *name; uint32_t colour; } CTL[6] = {
    { LV_SYMBOL_BLUETOOTH, "Bluetooth", 0x0A84FF }, { LV_SYMBOL_MUTE, "Ruhe", 0x5E5CE6 },
    { LV_SYMBOL_EYE_OPEN, "Immer an", 0x30D158 },   { LV_SYMBOL_CHARGE, "Lampe", 0xFFD60A },
    { LV_SYMBOL_IMAGE, "Helligkeit", 0xFF9F0A },     { LV_SYMBOL_SETTINGS, "Mehr", 0x8E8E93 },
};

static void ctl_toggle(lv_event_t *e) {
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    if (i == 0) st->bluetooth_on ^= 1;
    if (i == 1) st->dnd ^= 1;
    if (i == 2) st->aod ^= 1;
    if (i == 3) st->flashlight ^= 1;
    if (i == 4) st->brightness = st->brightness >= 100 ? 20 : st->brightness + 40 > 100 ? 100 : st->brightness + 40;
    ow_ui_update();
}

static void control_create(lv_obj_t *t) {
    ctl_time = label(t, &ow_font_64, 0xFFFFFF, "");
    lv_obj_align(ctl_time, LV_ALIGN_TOP_MID, 0, 22);
    ctl_date = label(t, &ow_font_20, 0x98989D, "");
    lv_obj_align(ctl_date, LV_ALIGN_TOP_MID, 0, 102);
    for (int i = 0; i < 6; i++) {
        lv_obj_t *b = lv_obj_create(t);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 96, 96);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_pos(b, 22 + (i % 3) * 112, 150 + (i / 3) * 136);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, ctl_toggle, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *s = label(b, &ow_font_28, 0xFFFFFF, CTL[i].symbol);
        lv_obj_center(s);
        lv_obj_t *n = label(t, &ow_font_20, 0xC7C7CC, CTL[i].name);
        lv_obj_set_width(n, 112);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(n, 22 + (i % 3) * 112 - 8, 150 + (i / 3) * 136 + 102);
        ctl_btn[i] = b;
    }
}

/* ---- activity ---- */

static const struct { const char *name; uint32_t colour; } ACT[3] = {
    { "Schritte", 0x30D158 }, { "kcal", 0xFF375F }, { "Puls", 0xFF9F0A } };

static void activity_create(lv_obj_t *t) {
    lv_obj_t *title = label(t, &ow_font_28, 0xFFFFFF, "Heute");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *arc = lv_arc_create(t);
        int size = 250 - i * 56;
        lv_obj_set_size(arc, size, size);
        lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 70 + i * 28);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_rotation(arc, 270);
        lv_obj_remove_style(arc, 0, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_darken(hex(ACT[i].colour), 190), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, hex(ACT[i].colour), LV_PART_INDICATOR);
        act_arc[i] = arc;
        lv_obj_t *v = label(t, &ow_font_20, ACT[i].colour, "");
        lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 24 + i * 112, -48);
        act_val[i] = v;
        lv_obj_t *n = label(t, &ow_font_20, 0x98989D, ACT[i].name);
        lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 24 + i * 112, -20);
    }
}

/* ---- notifications ---- */

static void notes_create(lv_obj_t *t) {
    lv_obj_t *title = label(t, &ow_font_28, 0xFFFFFF, "Mitteilungen");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);
    note_box = lv_obj_create(t);
    lv_obj_remove_style_all(note_box);
    lv_obj_set_size(note_box, W, H - 76);
    lv_obj_set_y(note_box, 76);
    lv_obj_set_flex_flow(note_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(note_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(note_box, 12, 0);
}

static void notes_fill(void) {
    lv_obj_clean(note_box);
    if (!st->notes) {
        lv_obj_t *l = label(note_box, &ow_font_20, 0x98989D, "Keine Mitteilungen");
        lv_obj_set_style_pad_top(l, 140, 0);
        return;
    }
    for (int i = 0; i < st->notes && i < 3; i++) {
        lv_obj_t *card = lv_obj_create(note_box);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, W - 32, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(card, 28, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(card, hex(0x1C1C1E), 0);
        lv_obj_set_style_pad_all(card, 16, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        label(card, &ow_font_20, 0x0A84FF, st->note_app[i]);
        lv_obj_t *t = label(card, &ow_font_20, 0xFFFFFF, st->note_text[i]);
        lv_obj_set_width(t, W - 64);
        lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    }
}

/* ---- build and update ---- */

void ow_ui_create(ow_state *s) {
    st = s;
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    tv = lv_tileview_create(scr);
    lv_obj_set_style_bg_color(tv, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    tiles[OW_TILE_ACTIVITY] = lv_tileview_add_tile(tv, 0, 1, LV_DIR_RIGHT);
    tiles[OW_TILE_FACE] = lv_tileview_add_tile(tv, 1, 1, LV_DIR_ALL);
    tiles[OW_TILE_APPS] = lv_tileview_add_tile(tv, 2, 1, LV_DIR_LEFT);
    tiles[OW_TILE_CONTROL] = lv_tileview_add_tile(tv, 1, 0, LV_DIR_BOTTOM);
    tiles[OW_TILE_NOTES] = lv_tileview_add_tile(tv, 1, 2, LV_DIR_TOP);
    face = face_create(tiles[OW_TILE_FACE]);
    apps_create(tiles[OW_TILE_APPS]);
    control_create(tiles[OW_TILE_CONTROL]);
    activity_create(tiles[OW_TILE_ACTIVITY]);
    notes_create(tiles[OW_TILE_NOTES]);
    ow_ui_show(OW_TILE_FACE);
    ow_ui_update();
}

void ow_ui_show(int tile) {
    static const uint8_t col[5] = { 0, 1, 2, 1, 1 }, row[5] = { 1, 1, 1, 0, 2 };
    lv_obj_set_tile_id(tv, col[tile], row[tile], LV_ANIM_OFF);
}

void ow_ui_update(void) {
    static const char *const WD[7] = { "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag" };
    char t[48];
    const owf_values *v = &st->v;
    lv_obj_invalidate(face);
    lv_label_set_text_fmt(ctl_time, "%02u:%02u", v->hour, v->minute);
    snprintf(t, sizeof t, "%s, %u.%u.  " LV_SYMBOL_BATTERY_FULL " %u %%", WD[v->weekday % 7], v->day, v->month, v->battery);
    lv_label_set_text(ctl_date, t);
    const uint8_t on[6] = { st->bluetooth_on, st->dnd, st->aod, st->flashlight, 1, 0 };
    for (int i = 0; i < 6; i++)
        lv_obj_set_style_bg_color(ctl_btn[i], on[i] ? hex(CTL[i].colour) : hex(0x2C2C2E), 0);
    uint32_t val[3] = { v->steps, v->kcal, v->heart }, goal[3] = { v->step_goal, v->kcal_goal, 200 };
    for (int i = 0; i < 3; i++) {
        lv_arc_set_range(act_arc[i], 0, 1000);
        lv_arc_set_value(act_arc[i], goal[i] ? (int16_t)(val[i] * 1000u / goal[i] > 1000u ? 1000u : val[i] * 1000u / goal[i]) : 0);
        lv_label_set_text_fmt(act_val[i], "%lu", (unsigned long)val[i]);
    }
    notes_fill();
}
