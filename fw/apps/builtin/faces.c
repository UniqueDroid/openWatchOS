/* SPDX-License-Identifier: GPL-2.0-only */
/* Watch faces: browse the faces in faces/ (stock format), each shown as a half-size preview drawn by the owf
 * engine; "Verwenden" makes it the current face. */
#include "../ow_app.h"
#include "../../owf/owf.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXF 24
static char names[MAXF][32];
static int nf, cur;
static lv_obj_t *img, *name_l, *use_b;
static lv_draw_buf_t *preview;

static void add(const char *n, int is_dir, void *u) {
    LV_UNUSED(u);
    size_t l = strlen(n);
    if (!is_dir && nf < MAXF && l > 3 && l < 32 && !strcmp(n + l - 3, ".di")) snprintf(names[nf++], 32, "%s", n);
}

static void render(void) {
    if (!nf || !preview) return;
    char path[64];
    snprintf(path, sizeof path, "faces/%s", names[cur]);
    int n = ow_sys_size(path);
    uint8_t *d = n > 0 ? malloc((size_t)n) : 0;
    lv_memzero(preview->data, OW_W * OW_H * 2);
    if (d && ow_sys_read(path, d, (unsigned)n) == n && !owf_check(d, (uint32_t)n)) {
        owf_band b = { (uint16_t *)preview->data, 0, OWF_H, 0 };
        owf_render(d, (uint32_t)n, &ow_ui_state()->v, &b, 0);
    }
    free(d);
    lv_image_set_src(img, preview);
    lv_obj_invalidate(img);
    char t[40];
    snprintf(t, sizeof t, "%.*s  (%d/%d)", (int)strlen(names[cur]) - 3, names[cur], cur + 1, nf);
    lv_label_set_text(name_l, t);
    int active = !strcmp(names[cur], ow_settings_get()->face);
    lv_obj_set_style_bg_color(use_b, lv_color_hex(active ? 0x3A3A3C : OW_BLUE), 0);
    lv_label_set_text(lv_obj_get_child(use_b, 0), active ? OW_ICON_CHECK " Aktiv" : "Verwenden");
}

static void on_step(lv_event_t *e) {
    if (!nf) return;
    cur = (cur + (int)(intptr_t)lv_event_get_user_data(e) + nf) % nf;
    render();
}

static void on_use(lv_event_t *e) {
    LV_UNUSED(e);
    if (!nf) return;
    snprintf(ow_settings_get()->face, sizeof ow_settings_get()->face, "%s", names[cur]);
    ow_settings_save();
    ow_ui_load_face();
    ow_ui_update();
    render();
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    nf = 0; cur = 0;
    ow_sys_list("faces", add, 0);
    for (int i = 0; i < nf; i++) if (!strcmp(names[i], ow_settings_get()->face)) cur = i;
    lv_obj_t *t = ow_label(scr, &ow_font_28, 0xAC8E68, "Zifferblätter");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
    if (!preview) preview = lv_draw_buf_create(OW_W, OW_H, LV_COLOR_FORMAT_RGB565, OW_W * 2);
    lv_obj_t *frame = lv_obj_create(scr);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, OW_W / 2 + 8, OW_H / 2 + 8);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_radius(frame, 30, 0);
    lv_obj_set_style_border_width(frame, 4, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_clip_corner(frame, true, 0);
    lv_obj_set_scrollable(frame, false);
    img = lv_image_create(frame);
    lv_image_set_scale(img, 128);
    lv_obj_set_size(img, OW_W / 2, OW_H / 2);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
    lv_obj_center(img);
    lv_obj_t *l = ow_button(scr, 64, 0x2C2C2E, LV_SYMBOL_LEFT, on_step, (void *)(intptr_t)-1);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 12, 150);
    lv_obj_t *r = ow_button(scr, 64, 0x2C2C2E, LV_SYMBOL_RIGHT, on_step, (void *)(intptr_t)1);
    lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -12, 150);
    name_l = ow_label(scr, &ow_font_20, 0xFFFFFF, "");
    lv_obj_align(name_l, LV_ALIGN_TOP_MID, 0, 300);
    use_b = lv_obj_create(scr);
    lv_obj_remove_style_all(use_b);
    lv_obj_set_size(use_b, 220, 64);
    lv_obj_align(use_b, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_radius(use_b, 32, 0);
    lv_obj_set_style_bg_opa(use_b, LV_OPA_COVER, 0);
    lv_obj_set_clickable(use_b, true);
    lv_obj_add_event_cb(use_b, on_use, LV_EVENT_SHORT_CLICKED, 0);
    lv_obj_t *ul = ow_label(use_b, &ow_font_28, 0xFFFFFF, "");
    lv_obj_center(ul);
    if (!nf) { lv_label_set_text(name_l, "Keine Zifferblätter in faces/"); return; }
    render();
}

const ow_app ow_app_faces = { "faces", "Zifferblätter", OW_ICON_PALETTE, 0xAC8E68, open, 0, 0 };
