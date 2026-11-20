/* SPDX-License-Identifier: GPL-2.0-only */
#include "ow_app.h"
#include "../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEPTH 4
static struct { const ow_app *app; lv_obj_t *scr; } stack[DEPTH];
static int depth;

static void gesture(lv_event_t *e) {
    LV_UNUSED(e);
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) ow_app_back();
}

void ow_app_open(const ow_app *a, const char *arg) {
    if (!a || depth == DEPTH) return;
    lv_obj_t *scr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_clickable(scr, true);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, gesture, LV_EVENT_GESTURE, 0);
    stack[depth].app = a; stack[depth].scr = scr; depth++;
    a->open(scr, arg);
}

void ow_app_back(void) {
    if (!depth) return;
    depth--;
    if (stack[depth].app->close) stack[depth].app->close();
    lv_obj_delete(stack[depth].scr);
}

int ow_app_depth(void) { return depth; }
const ow_app *ow_app_top(void) { return depth ? stack[depth - 1].app : 0; }
void ow_app_tick(void) { if (depth && stack[depth - 1].app->tick) stack[depth - 1].app->tick(); }

extern const ow_app ow_app_stopwatch, ow_app_timer, ow_app_alarm, ow_app_lamp, ow_app_settings, ow_app_faces,
    ow_app_weather, ow_app_music, ow_app_heart, ow_app_store, ow_app_jsapps;
static const ow_app *const BUILTIN[] = { &ow_app_heart, &ow_app_weather, &ow_app_music, &ow_app_stopwatch, &ow_app_timer,
                                         &ow_app_alarm, &ow_app_lamp, &ow_app_faces, &ow_app_store, &ow_app_jsapps,
                                         &ow_app_settings };

const ow_app *const *ow_app_builtin(unsigned *count) { *count = sizeof BUILTIN / sizeof BUILTIN[0]; return BUILTIN; }

const ow_app *ow_app_find(const char *id) {
    for (unsigned i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++) if (!strcmp(BUILTIN[i]->id, id)) return BUILTIN[i];
    return 0;
}

/* ---- settings: "key=value" lines ---- */

static ow_settings settings = { .brightness = 60, .aod = 1, .dnd = 0, .vibration = 1, .lang = 0, .h12 = 0,
                                .raise_to_wake = 1, .face = "skynet.di" };

ow_settings *ow_settings_get(void) { return &settings; }

void ow_settings_load(void) {
    char buf[512];
    int n = ow_sys_read("sys/settings.txt", buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = 0;
    for (char *line = strtok(buf, "\n"); line; line = strtok(0, "\n")) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *v = eq + 1;
        int x = atoi(v);
        if (!strcmp(line, "brightness")) settings.brightness = (uint8_t)(x > 100 ? 100 : x < 5 ? 5 : x);
        else if (!strcmp(line, "aod")) settings.aod = x != 0;
        else if (!strcmp(line, "dnd")) settings.dnd = x != 0;
        else if (!strcmp(line, "vibration")) settings.vibration = x != 0;
        else if (!strcmp(line, "lang")) settings.lang = x != 0;
        else if (!strcmp(line, "h12")) settings.h12 = x != 0;
        else if (!strcmp(line, "raise")) settings.raise_to_wake = x != 0;
        else if (!strcmp(line, "face")) snprintf(settings.face, sizeof settings.face, "%s", v);
    }
}

void ow_settings_save(void) {
    char buf[512];
    int n = snprintf(buf, sizeof buf, "brightness=%u\naod=%u\ndnd=%u\nvibration=%u\nlang=%u\nh12=%u\nraise=%u\nface=%s\n",
                     settings.brightness, settings.aod, settings.dnd, settings.vibration, settings.lang, settings.h12,
                     settings.raise_to_wake, settings.face);
    ow_sys_write("sys/settings.txt", buf, (unsigned)n);
}
