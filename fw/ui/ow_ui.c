/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS user interface, see ow_ui.h. Dark, round, large touch targets; all colours on black (AMOLED). */
#include "ow_ui.h"
#include "ow_widgets.h"
#include "../apps/ow_app.h"
#include "../apps/ow_pkg.h"
#include "../sys/ow_sys.h"
#include "lvgl_private.h"   /* image decoder descriptor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W OW_W
#define H OW_H

static ow_state *st;
static lv_obj_t *tv, *tiles[5], *face, *ctl_time, *ctl_date, *ctl_btn[6], *act_arc[3], *act_val[3], *note_box, *app_list;

static lv_color_t hex(uint32_t c) { return lv_color_hex(c); }
ow_state *ow_ui_state(void) { return st; }

/* ---- watch face: an image decoder that lets the owf engine draw the rows LVGL asks for ---- */

/* The image source is an lv_image_dsc_t only this decoder accepts; it hands LVGL bands of OWF_BAND full-width
 * rows (RGB565). No screen buffer: a stock watch face costs one band of RAM. */
#define OWF_BAND 40
static const char FACE_MARK[4] = { 'O', 'W', 'F', '1' };
static lv_image_dsc_t face_src = { .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RAW, .w = W, .h = H },
                                   .data = (const uint8_t *)FACE_MARK, .data_size = sizeof FACE_MARK };

static int is_face(const void *src, lv_image_src_t type) { return type == LV_IMAGE_SRC_VARIABLE && src == &face_src; }

static lv_result_t face_info(lv_image_decoder_t *d, lv_image_decoder_dsc_t *dsc, lv_image_header_t *h) {
    LV_UNUSED(d);
    if (!is_face(dsc->src, dsc->src_type)) return LV_RESULT_INVALID;
    lv_memzero(h, sizeof *h);
    h->magic = LV_IMAGE_HEADER_MAGIC; h->cf = LV_COLOR_FORMAT_RGB565; h->w = W; h->h = H; h->stride = W * 2;
    return LV_RESULT_OK;
}

static lv_result_t face_open(lv_image_decoder_t *d, lv_image_decoder_dsc_t *dsc) {
    LV_UNUSED(d);
    if (!is_face(dsc->src, dsc->src_type)) return LV_RESULT_INVALID;
    dsc->decoded = NULL;   /* drawn in bands by face_area */
    return LV_RESULT_OK;
}

static lv_result_t face_area(lv_image_decoder_t *d, lv_image_decoder_dsc_t *dsc, const lv_area_t *full, lv_area_t *done) {
    LV_UNUSED(d);
    lv_draw_buf_t *buf = (lv_draw_buf_t *)dsc->decoded;
    if (done->y1 == LV_COORD_MIN) {
        if (!buf) buf = lv_draw_buf_create(W, OWF_BAND, LV_COLOR_FORMAT_RGB565, W * 2);
        if (!buf) return LV_RESULT_INVALID;
        dsc->decoded = buf;
        done->y1 = full->y1;
    } else done->y1 = done->y2 + 1;
    if (done->y1 > full->y2) return LV_RESULT_INVALID;
    done->x1 = 0; done->x2 = W - 1;   /* always whole rows: the engine draws full width */
    done->y2 = done->y1 + OWF_BAND - 1 < full->y2 ? done->y1 + OWF_BAND - 1 : full->y2;
    owf_band b = { (uint16_t *)buf->data, done->y1, done->y2 - done->y1 + 1, 0 };
    lv_memzero(buf->data, (size_t)b.rows * W * 2);
    if (st && st->face) owf_render(st->face, st->face_len, &st->v, &b, 0);
    return LV_RESULT_OK;
}

static void face_close(lv_image_decoder_t *d, lv_image_decoder_dsc_t *dsc) {
    LV_UNUSED(d);
    if (dsc->decoded) lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    dsc->decoded = NULL;
}

static void face_tap(lv_event_t *e) {
    LV_UNUSED(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    int action = owf_tap(st->face, st->face_len, p.x, p.y);
    /* tap actions of the stock faces: 1 steps, 2 pulse, 6 weather, 10 music, 11 alarm, 12 settings */
    const char *id = action == 2 ? "heart" : action == 6 ? "weather" : action == 10 ? "music" : action == 11 ? "alarm"
                   : action == 12 ? "settings" : 0;
    if (action == 1) ow_ui_show(OW_TILE_ACTIVITY);
    else if (id) ow_app_open(ow_app_find(id), 0);
}

static lv_obj_t *face_create(lv_obj_t *parent) {
    static lv_image_decoder_t *dec;
    if (!dec) {
        dec = lv_image_decoder_create();
        lv_image_decoder_set_info_cb(dec, face_info);
        lv_image_decoder_set_open_cb(dec, face_open);
        lv_image_decoder_set_get_area_cb(dec, face_area);
        lv_image_decoder_set_close_cb(dec, face_close);
    }
    lv_obj_t *o = lv_image_create(parent);
    lv_image_set_src(o, &face_src);
    lv_obj_set_clickable(o, true);
    lv_obj_set_scrollable(o, false);
    lv_obj_add_event_cb(o, face_tap, LV_EVENT_SHORT_CLICKED, 0);
    return o;
}

void ow_ui_load_face(void) {
    static uint8_t *data;
    char path[64];
    snprintf(path, sizeof path, "faces/%s", ow_settings_get()->face);
    int n = ow_sys_size(path);
    if (n > 0) {
        uint8_t *d = malloc((size_t)n);
        if (d && ow_sys_read(path, d, (unsigned)n) == n && !owf_check(d, (uint32_t)n)) {
            free(data); data = d;
            st->face = data; st->face_len = (uint32_t)n;
            if (face) lv_image_cache_drop(&face_src);
            return;
        }
        free(d);
    }
}

/* ---- apps ---- */

static void open_app(lv_event_t *e) { ow_app_open((const ow_app *)lv_event_get_user_data(e), 0); }
static void open_js(lv_event_t *e);

void ow_ui_apps_changed(void) {
    lv_obj_clean(app_list);
    unsigned n;
    const ow_app *const *apps = ow_app_builtin(&n);
    for (unsigned i = 0; i < n; i++) {
        lv_obj_t *row = ow_row(app_list, apps[i]->symbol, apps[i]->colour, apps[i]->name, 0);
        lv_obj_add_event_cb(row, open_app, LV_EVENT_SHORT_CLICKED, (void *)apps[i]);
    }
    /* installed JS apps that are switched to the main list */
    static ow_pkg_info pk[OW_PKG_MAX];
    int k = ow_pkg_list(pk, OW_PKG_MAX);
    for (int i = 0; i < k; i++) {
        if (!pk[i].pinned || strcmp(pk[i].type, "app")) continue;
        lv_obj_t *row = ow_row(app_list, OW_ICON_CODE, pk[i].colour, pk[i].name, 0);
        lv_obj_add_event_cb(row, open_js, LV_EVENT_SHORT_CLICKED, pk[i].id);
    }
}

extern const ow_app ow_app_js;
static void open_js(lv_event_t *e) { ow_app_open(&ow_app_js, (const char *)lv_event_get_user_data(e)); }

static void apps_create(lv_obj_t *t) {
    app_list = lv_obj_create(t);
    lv_obj_remove_style_all(app_list);
    lv_obj_set_size(app_list, W, H);
    lv_obj_set_flex_flow(app_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(app_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(app_list, 70, 0);
    lv_obj_set_style_pad_row(app_list, 10, 0);
    lv_obj_set_scroll_dir(app_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(app_list, LV_SCROLLBAR_MODE_OFF);
    ow_ui_apps_changed();
}

/* ---- control centre ---- */

static const struct { const char *symbol, *name; uint32_t colour; } CTL[6] = {
    { LV_SYMBOL_BLUETOOTH, "Bluetooth", OW_BLUE }, { OW_ICON_MOON, "Ruhe", 0x5E5CE6 },
    { LV_SYMBOL_EYE_OPEN, "Immer an", OW_GREEN },  { OW_ICON_LAMP, "Lampe", OW_YELLOW },
    { OW_ICON_SUN, "Helligkeit", OW_ORANGE },      { OW_ICON_COG, "Mehr", 0x8E8E93 },
};

static void ctl_toggle(lv_event_t *e) {
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    ow_settings *s = ow_settings_get();
    if (i == 0) st->bluetooth_on ^= 1;
    if (i == 1) s->dnd ^= 1;
    if (i == 2) s->aod ^= 1;
    if (i == 3) { ow_app_open(ow_app_find("lamp"), 0); return; }
    if (i == 4) { s->brightness = s->brightness >= 100 ? 20 : s->brightness + 40 > 100 ? 100 : (uint8_t)(s->brightness + 40); ow_sys_brightness(s->brightness); }
    if (i == 5) { ow_app_open(ow_app_find("settings"), 0); return; }
    ow_settings_save();
    ow_ui_update();
}

static void control_create(lv_obj_t *t) {
    ctl_time = ow_label(t, &ow_font_64, 0xFFFFFF, "");
    lv_obj_align(ctl_time, LV_ALIGN_TOP_MID, 0, 22);
    ctl_date = ow_label(t, &ow_font_20, OW_GREY, "");
    lv_obj_align(ctl_date, LV_ALIGN_TOP_MID, 0, 102);
    for (int i = 0; i < 6; i++) {
        lv_obj_t *b = ow_button(t, 96, 0x2C2C2E, CTL[i].symbol, ctl_toggle, (void *)(intptr_t)i);
        lv_obj_set_pos(b, 22 + (i % 3) * 112, 150 + (i / 3) * 136);
        lv_obj_t *n = ow_label(t, &ow_font_20, 0xC7C7CC, CTL[i].name);
        lv_obj_set_width(n, 112);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(n, 22 + (i % 3) * 112 - 8, 150 + (i / 3) * 136 + 102);
        ctl_btn[i] = b;
    }
}

/* ---- activity ---- */

static const struct { const char *name; uint32_t colour; } ACT[3] = {
    { "Schritte", OW_GREEN }, { "kcal", OW_RED }, { "Puls", OW_ORANGE } };

static void activity_create(lv_obj_t *t) {
    lv_obj_t *title = ow_label(t, &ow_font_28, 0xFFFFFF, "Heute");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *arc = lv_arc_create(t);
        int size = 250 - i * 56;
        lv_obj_set_size(arc, size, size);
        lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 70 + i * 28);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_rotation(arc, 270);
        lv_obj_remove_style(arc, 0, LV_PART_KNOB);
        lv_obj_set_clickable(arc, false);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_darken(hex(ACT[i].colour), 190), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, hex(ACT[i].colour), LV_PART_INDICATOR);
        act_arc[i] = arc;
        lv_obj_t *v = ow_label(t, &ow_font_20, ACT[i].colour, "");
        lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 24 + i * 112, -48);
        act_val[i] = v;
        lv_obj_t *n = ow_label(t, &ow_font_20, OW_GREY, ACT[i].name);
        lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 24 + i * 112, -20);
    }
}

/* ---- notifications ---- */

static void notes_create(lv_obj_t *t) {
    note_box = ow_page(t, "Mitteilungen", 0xFFFFFF);
}

static void notes_fill(void) {
    lv_obj_clean(note_box);
    if (!st->notes) {
        lv_obj_t *l = ow_label(note_box, &ow_font_20, OW_GREY, "Keine Mitteilungen");
        lv_obj_set_style_pad_top(l, 140, 0);
        return;
    }
    for (int i = 0; i < st->notes && i < 3; i++) {
        lv_obj_t *card = lv_obj_create(note_box);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, W - 32, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(card, 28, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(card, hex(OW_CARD), 0);
        lv_obj_set_style_pad_all(card, 16, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        ow_label(card, &ow_font_20, OW_BLUE, st->note_app[i]);
        lv_obj_t *t = ow_label(card, &ow_font_20, 0xFFFFFF, st->note_text[i]);
        lv_obj_set_width(t, W - 64);
        lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    }
}

/* ---- build and update ---- */

void ow_ui_create(ow_state *s) {
    st = s;
    ow_settings_load();
    ow_ui_load_face();
    lv_obj_t *scr = lv_screen_active();
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
    lv_tileview_set_tile_by_index(tv, col[tile], row[tile], LV_ANIM_OFF);
}

void ow_ui_update(void) {
    static const char *const WD[7] = { "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag" };
    char t[48];
    owf_values *v = &st->v;
    ow_time tm;
    ow_time_split(ow_sys_seconds(), &tm);
    v->year = tm.year; v->month = tm.month; v->day = tm.day; v->weekday = tm.weekday;
    v->hour = tm.hour; v->minute = tm.minute; v->second = tm.second;
    v->h12 = ow_settings_get()->h12; v->battery = (uint8_t)ow_sys_battery(); v->ms = ow_sys_ms();
    lv_obj_invalidate(face);   /* new values: LVGL asks the decoder for the bands again */
    lv_label_set_text_fmt(ctl_time, "%02u:%02u", v->hour, v->minute);
    snprintf(t, sizeof t, "%s, %u.%u.  " LV_SYMBOL_BATTERY_FULL " %u %%", WD[v->weekday % 7], v->day, v->month, v->battery);
    lv_label_set_text(ctl_date, t);
    ow_settings *s = ow_settings_get();
    const uint8_t on[6] = { st->bluetooth_on, s->dnd, s->aod, st->flashlight, 1, 0 };
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
