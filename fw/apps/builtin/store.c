/* SPDX-License-Identifier: GPL-2.0-only */
/* App store: the catalogue comes from the phone (sys/store.json), packages too (the phone fetches them from the
 * store server and hands them over). The watch never talks to the internet itself.
 *
 *   watch -> phone, channel "store":  "catalog"           send the current catalogue
 *                                     "get <id>"          send this package
 *   phone -> watch:                   ow_store_catalog()  JSON {"apps":[{"id","name","version","author",
 *                                                         "description","size","permissions":[...]}]}
 *                                     ow_store_package()  the .oapps bytes
 * Detail page: what the app may do (permissions in plain words), install / update / remove / open / main list. */
#include "../ow_app.h"
#include "../ow_json.h"
#include "../ow_pkg.h"
#include "../../ui/ow_ui.h"
#include "../../ui/ow_widgets.h"
#include "../../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAT_MAX 32
typedef struct cat { char id[48], name[24], version[16], author[24], description[160]; uint32_t size, permissions; } cat;
static cat apps[CAT_MAX];
static int napps;
static char waiting[48], status[64];
static lv_obj_t *list, *status_l, *detail_status;

static const char *const PERM_TEXT[14] = { "Uhrzeit und Datum", "Regelmäßig neu zeichnen", "Vibration",
    "Bildschirm länger an", "Eigene Daten speichern", "Akkustand", "Schritte", "Puls", "Internet über das Handy",
    "Mitteilungen anzeigen", "Nachrichten mit dem Handy", "Lautstärke am Mikrofon", "Speicher lesen (Diagnose)",
    "Eigene Programme starten" };

static void load_catalog(void) {
    static char buf[16384];
    napps = 0;
    int n = ow_sys_read("sys/store.json", buf, sizeof buf);
    if (n <= 0) return;
    const char *end = buf + n, *arr = ow_json_get(buf, end, "apps");
    for (const char *e = arr ? ow_json_first(arr, end) : 0; e && napps < CAT_MAX; e = ow_json_next(e, end)) {
        cat *c = &apps[napps];
        memset(c, 0, sizeof *c);
        if (ow_json_str(ow_json_get(e, end, "id"), end, c->id, sizeof c->id) <= 0) continue;
        ow_json_str(ow_json_get(e, end, "name"), end, c->name, sizeof c->name);
        ow_json_str(ow_json_get(e, end, "version"), end, c->version, sizeof c->version);
        ow_json_str(ow_json_get(e, end, "author"), end, c->author, sizeof c->author);
        ow_json_str(ow_json_get(e, end, "description"), end, c->description, sizeof c->description);
        c->size = (uint32_t)ow_json_num(ow_json_get(e, end, "size"), end, 0);
        const char *p = ow_json_get(e, end, "permissions");
        for (const char *q = p ? ow_json_first(p, end) : 0; q; q = ow_json_next(q, end)) {
            char name[24]; ow_json_str(q, end, name, sizeof name);
            for (int k = 0; k < 14; k++) if (ow_pkg_permission_names[k] && !strcmp(name, ow_pkg_permission_names[k])) c->permissions |= 1u << k;
        }
        napps++;
    }
}

/* 0 not installed, 1 installed and current, 2 update available */
static int state_of(const cat *c, ow_pkg_info *info) {
    if (ow_pkg_get(c->id, info)) return 0;
    return ow_pkg_version_cmp(info->version, c->version) < 0 ? 2 : 1;
}

static void fill(void);
static void detail_update(const cat *c);

void ow_store_catalog(const char *json, unsigned len) {
    ow_sys_write("sys/store.json", json, len);
    load_catalog();
    snprintf(status, sizeof status, "%d Apps im Store", napps);
    if (list) fill();
}

void ow_store_package(const char *id, const uint8_t *tar, unsigned len) {
    ow_pkg_info info;
    int r = ow_pkg_install(tar, len, &info);
    if (!r && strcmp(info.id, id)) r = OW_PKG_BAD_APPJSON;   /* not the package that was asked for */
    snprintf(status, sizeof status, "%s: %s", r ? id : info.name, ow_pkg_error(r));
    waiting[0] = 0;
    if (list) fill();
    if (detail_status) {
        for (int i = 0; i < napps; i++) if (!strcmp(apps[i].id, id)) detail_update(&apps[i]);
        lv_label_set_text(detail_status, ow_pkg_error(r));
    }
    ow_ui_apps_changed();
}

static void ask(const char *id) {
    char m[64]; int n = snprintf(m, sizeof m, "get %s", id);
    snprintf(waiting, sizeof waiting, "%s", id);
    if (detail_status) lv_label_set_text(detail_status, "Wird geladen …");
    ow_sys_send("store", m, (unsigned)n);
}

/* ---- detail ---- */

static const cat *shown;
extern const ow_app ow_app_js;

static void on_main(lv_event_t *e) {
    LV_UNUSED(e);
    ow_pkg_info info;
    int st = state_of(shown, &info);
    if (st == 1) { ow_app_open(&ow_app_js, shown->id); return; }
    ask(shown->id);
}

static void on_remove(lv_event_t *e) {
    LV_UNUSED(e);
    ow_pkg_remove(shown->id);
    lv_label_set_text(detail_status, "Entfernt");
    detail_update(shown);
    if (list) fill();
    ow_ui_apps_changed();
}

static void on_pin(lv_event_t *e) {
    ow_pkg_pin(shown->id, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    ow_ui_apps_changed();
}

static lv_obj_t *btn_main, *btn_remove, *row_pin;

static void detail_update(const cat *c) {
    ow_pkg_info info;
    int st = state_of(c, &info);
    lv_label_set_text(lv_obj_get_child(btn_main, 0), st == 0 ? OW_ICON_DOWNLOAD " Installieren" : st == 2 ? OW_ICON_SYNC " Aktualisieren" : OW_ICON_PLAY " Öffnen");
    lv_obj_set_style_bg_color(btn_main, lv_color_hex(st == 1 ? OW_GREEN : OW_BLUE), 0);
    lv_obj_set_hidden(btn_remove, st == 0);
    lv_obj_set_hidden(row_pin, st == 0);
    if (st) lv_obj_set_state(lv_obj_get_child(row_pin, 2), LV_STATE_CHECKED, info.pinned);
}

static lv_obj_t *pill(lv_obj_t *p, uint32_t colour, lv_event_cb_t cb) {
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, OW_W - 64, 64);
    lv_obj_set_style_radius(b, 32, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(colour), 0);
    lv_obj_set_clickable(b, true);
    lv_obj_set_scrollable(b, false);
    lv_obj_add_event_cb(b, cb, LV_EVENT_SHORT_CLICKED, 0);
    lv_obj_t *l = ow_label(b, &ow_font_28, 0xFFFFFF, "");
    lv_obj_center(l);
    return b;
}

static void open_detail(lv_obj_t *scr, const char *id) {
    shown = 0;
    for (int i = 0; i < napps; i++) if (!strcmp(apps[i].id, id)) shown = &apps[i];
    if (!shown) return;
    lv_obj_t *box = ow_page(scr, shown->name, 0xFFFFFF);
    char t[96];
    snprintf(t, sizeof t, "%s · %s · %lu KB", shown->author[0] ? shown->author : "?", shown->version, (unsigned long)(shown->size + 1023u) / 1024u);
    ow_label(box, &ow_font_20, OW_GREY, t);
    lv_obj_t *d = ow_label(box, &ow_font_20, 0xFFFFFF, shown->description);
    lv_obj_set_width(d, OW_W - 56);
    lv_label_set_long_mode(d, LV_LABEL_LONG_MODE_WRAP);
    btn_main = pill(box, OW_BLUE, on_main);
    detail_status = ow_label(box, &ow_font_20, OW_GREY, "");
    lv_obj_t *h = ow_label(box, &ow_font_20, OW_GREY, shown->permissions ? "Darf:" : "Braucht keine Berechtigungen");
    LV_UNUSED(h);
    for (int k = 0; k < 14; k++)
        if (shown->permissions >> k & 1u) { snprintf(t, sizeof t, OW_ICON_CHECK "  %s", PERM_TEXT[k]); ow_label(box, &ow_font_20, 0xFFFFFF, t); }
    row_pin = ow_row(box, LV_SYMBOL_LIST, OW_BLUE, "Im Menü", 0);
    lv_obj_set_width(lv_obj_get_child(row_pin, 1), 150);
    lv_obj_t *sw = ow_switch(row_pin, 0, on_pin, 0);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -6, 0);
    btn_remove = pill(box, 0x3A3A3C, on_remove);
    lv_label_set_text(lv_obj_get_child(btn_remove, 0), OW_ICON_TRASH " Entfernen");
    detail_update(shown);
}
static void close_detail(void) { detail_status = 0; }
static const ow_app detail = { "store.detail", "App", OW_ICON_STORE, OW_BLUE, open_detail, close_detail, 0 };

/* ---- list ---- */

static void on_row(lv_event_t *e) { ow_app_open(&detail, ((const cat *)lv_event_get_user_data(e))->id); }
static void on_refresh(lv_event_t *e) { LV_UNUSED(e); snprintf(status, sizeof status, "Frage das Handy …"); ow_sys_send("store", "catalog", 7); fill(); }

static void fill(void) {
    lv_obj_clean(list);
    lv_obj_t *top = lv_obj_create(list);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, OW_W - 32, 56);
    status_l = ow_label(top, &ow_font_20, OW_GREY, status[0] ? status : napps ? "" : "Noch kein Katalog");
    lv_obj_set_width(status_l, OW_W - 120);
    lv_label_set_long_mode(status_l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(status_l, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_t *r = ow_button(top, 52, 0x2C2C2E, OW_ICON_SYNC, on_refresh, 0);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, -4, 0);
    for (int i = 0; i < napps; i++) {
        ow_pkg_info info;
        int st = state_of(&apps[i], &info);
        char right[24];
        if (!strcmp(waiting, apps[i].id)) snprintf(right, sizeof right, "lädt …");
        else if (st == 1) snprintf(right, sizeof right, OW_ICON_CHECK);
        else if (st == 2) snprintf(right, sizeof right, "Update");
        else snprintf(right, sizeof right, "%lu KB", (unsigned long)(apps[i].size + 1023u) / 1024u);
        uint32_t h = 2166136261u;
        for (const char *p = apps[i].id; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
        static const uint32_t C[8] = { 0x0A84FF, 0x30D158, 0xFF9F0A, 0xFF375F, 0xBF5AF2, 0x64D2FF, 0xFFD60A, 0xAC8E68 };
        lv_obj_t *row = ow_row(list, OW_ICON_CODE, C[h % 8u], apps[i].name, right);
        lv_obj_set_style_text_color(lv_obj_get_child(row, 2), lv_color_hex(st == 2 ? OW_BLUE : st == 1 ? OW_GREEN : OW_GREY), 0);
        lv_obj_add_event_cb(row, on_row, LV_EVENT_SHORT_CLICKED, &apps[i]);
    }
}

static void open(lv_obj_t *scr, const char *arg) {
    LV_UNUSED(arg);
    load_catalog();
    list = ow_page(scr, "App-Store", OW_BLUE);
    fill();
}
static void close(void) { list = 0; }

/* opens the store page of one app (from the phone, or a link in another app) */
void ow_store_show(const char *id) { load_catalog(); ow_app_open(&detail, id); }

const ow_app ow_app_store = { "store", "App-Store", OW_ICON_STORE, OW_BLUE, open, close, 0 };
