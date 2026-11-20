/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_APP_H
#define OW_APP_H
/* openWatchOS apps: built-in apps (C) and installed JS apps share one way to be opened and closed.
 * An app builds its page into a full-screen object; swiping right or the left key goes back. */
#include "lvgl.h"

typedef struct ow_app {
    const char *id, *name, *symbol;
    uint32_t colour;
    void (*open)(lv_obj_t *scr, const char *arg);   /* build the page */
    void (*close)(void);                             /* optional: before the page is deleted */
    void (*tick)(void);                              /* optional: every 100 ms while it is on top */
} ow_app;

void ow_app_open(const ow_app *a, const char *arg);
void ow_app_back(void);
int ow_app_depth(void);
const ow_app *ow_app_top(void);
void ow_app_tick(void);                              /* the main loop calls it every 100 ms */
const ow_app *ow_app_find(const char *id);
const ow_app *const *ow_app_builtin(unsigned *count);

/* settings, kept in sys/settings.txt */
typedef struct ow_settings {
    uint8_t brightness, aod, dnd, vibration, lang, h12, raise_to_wake;
    char face[32];        /* file in faces/ */
} ow_settings;
ow_settings *ow_settings_get(void);
void ow_settings_load(void);
void ow_settings_save(void);

#define OW_VERSION "0.1.0"

#endif
