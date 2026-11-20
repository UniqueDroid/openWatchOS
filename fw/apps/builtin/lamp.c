/* SPDX-License-Identifier: GPL-2.0-only */
/* Lamp: the whole screen lights at full brightness; tapping switches white, warm white, red. */
#include "../ow_app.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"

static const uint32_t COL[3] = { 0xFFFFFF, 0xFFD9A0, 0xFF2010 };
static int c;
static lv_obj_t *bg;

static void on_tap(lv_event_t *e) { LV_UNUSED(e); c = (c + 1) % 3; lv_obj_set_style_bg_color(bg, lv_color_hex(COL[c]), 0); }

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    bg = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL[c]), 0);
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_SHORT_CLICKED, 0);
    ow_sys_brightness(100);
    if (ow_ui_state()) ow_ui_state()->flashlight = 1;
}

static void close(void) {
    ow_sys_brightness(ow_settings_get()->brightness);
    if (ow_ui_state()) { ow_ui_state()->flashlight = 0; ow_ui_update(); }
}

const ow_app ow_app_lamp = { "lamp", "Lampe", OW_ICON_LAMP, OW_YELLOW, open, close, 0 };
