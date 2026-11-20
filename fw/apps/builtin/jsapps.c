/* SPDX-License-Identifier: GPL-2.0-only */
/* My apps: every installed JS app; tap opens it, the switch puts it into the main app list. */
#include "../ow_app.h"
#include "../ow_pkg.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include <string.h>

extern const ow_app ow_app_js;
static ow_pkg_info pk[OW_PKG_MAX];

static void on_open(lv_event_t *e) { ow_app_open(&ow_app_js, ((ow_pkg_info *)lv_event_get_user_data(e))->id); }
static void on_pin(lv_event_t *e) {
    ow_pkg_pin(((ow_pkg_info *)lv_event_get_user_data(e))->id, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    ow_ui_apps_changed();
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    lv_obj_t *box = ow_page(scr, "Meine Apps", 0x32ADE6);
    int n = ow_pkg_list(pk, OW_PKG_MAX), shown = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(pk[i].type, "app")) continue;
        lv_obj_t *row = ow_row(box, OW_ICON_CODE, pk[i].colour, pk[i].name, 0);
        lv_obj_set_width(lv_obj_get_child(row, 1), 150);
        lv_obj_add_event_cb(row, on_open, LV_EVENT_SHORT_CLICKED, &pk[i]);
        lv_obj_t *s = ow_switch(row, pk[i].pinned, on_pin, &pk[i]);
        lv_obj_align(s, LV_ALIGN_RIGHT_MID, -6, 0);
        shown++;
    }
    if (!shown) {
        lv_obj_t *l = ow_label(box, &ow_font_20, OW_GREY, "Noch keine Apps.\nIm App-Store gibt es welche.");
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *hint = ow_label(box, &ow_font_20, OW_GREY, "Schalter = im Hauptmenü");
    LV_UNUSED(hint);
}

const ow_app ow_app_jsapps = { "jsapps", "Meine Apps", OW_ICON_CODE, 0x32ADE6, open, 0, 0 };
