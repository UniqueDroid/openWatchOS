/* SPDX-License-Identifier: GPL-2.0-only */
/* Music remote: shows what the phone plays, sends play/pause, previous, next and volume to the phone. */
#include "../ow_app.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <string.h>

static lv_obj_t *title, *artist, *play, *vol;

static void show(void) {
    const ow_music *m = &ow_ui_state()->music;
    lv_label_set_text(title, m->title[0] ? m->title : "Keine Wiedergabe");
    lv_label_set_text(artist, m->artist);
    lv_label_set_text(lv_obj_get_child(play, 0), m->playing ? OW_ICON_PAUSE : OW_ICON_PLAY);
    lv_bar_set_value(vol, m->volume, LV_ANIM_OFF);
}

static void on_cmd(lv_event_t *e) {
    const char *cmd = lv_event_get_user_data(e);
    ow_music *m = &ow_ui_state()->music;
    if (!strcmp(cmd, "play")) m->playing ^= 1;
    if (!strcmp(cmd, "vol+")) m->volume = (uint8_t)(m->volume + 10 > 100 ? 100 : m->volume + 10);
    if (!strcmp(cmd, "vol-")) m->volume = (uint8_t)(m->volume < 10 ? 0 : m->volume - 10);
    ow_sys_send("music", cmd, (unsigned)strlen(cmd));
    show();
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *ic = ow_round_icon(scr, 96, 0xFF2D55, OW_ICON_MUSIC);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 22);
    title = ow_label(scr, &ow_font_28, 0xFFFFFF, "");
    lv_obj_set_width(title, OW_W - 48);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 132);
    artist = ow_label(scr, &ow_font_20, OW_GREY, "");
    lv_obj_align(artist, LV_ALIGN_TOP_MID, 0, 172);
    lv_obj_t *p = ow_button(scr, 80, 0x2C2C2E, OW_ICON_PREV, on_cmd, "prev");
    lv_obj_align(p, LV_ALIGN_TOP_LEFT, 30, 228);
    play = ow_button(scr, 112, 0xFF2D55, OW_ICON_PLAY, on_cmd, "play");
    lv_obj_align(play, LV_ALIGN_TOP_MID, 0, 212);
    lv_obj_t *n = ow_button(scr, 80, 0x2C2C2E, OW_ICON_NEXT, on_cmd, "next");
    lv_obj_align(n, LV_ALIGN_TOP_RIGHT, -30, 228);
    lv_obj_t *vm = ow_button(scr, 56, 0x1C1C1E, OW_ICON_VOL_DOWN, on_cmd, "vol-");
    lv_obj_align(vm, LV_ALIGN_BOTTOM_LEFT, 30, -34);
    lv_obj_t *vp = ow_button(scr, 56, 0x1C1C1E, OW_ICON_VOL_UP, on_cmd, "vol+");
    lv_obj_align(vp, LV_ALIGN_BOTTOM_RIGHT, -30, -34);
    vol = lv_bar_create(scr);
    lv_obj_set_size(vol, 160, 10);
    lv_obj_align(vol, LV_ALIGN_BOTTOM_MID, 0, -57);
    lv_bar_set_range(vol, 0, 100);
    lv_obj_set_style_bg_color(vol, lv_color_hex(0x39393D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(vol, lv_color_white(), LV_PART_INDICATOR);
    show();
}

const ow_app ow_app_music = { "music", "Musik", OW_ICON_MUSIC, 0xFF2D55, open, 0, 0 };
