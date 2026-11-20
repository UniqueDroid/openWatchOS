/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_UI_H
#define OW_UI_H
/* openWatchOS user interface on LVGL 9.6.
 *
 * Layout (swipe like the stock firmware):
 *                 control centre
 *                       ^ (swipe down from the face)
 *   activity  <-   watch face   ->  apps
 *                       v (swipe up)
 *                 notifications
 * The watch face is a stock watch face file ("JL" format, fw/owf), handed to LVGL in bands by an image decoder.
 * Apps open on top (fw/apps); swiping right goes back.
 */
#include "lvgl.h"
#include "../owf/owf.h"

typedef struct ow_weather {
    int16_t temp, low, high;
    uint8_t code;                 /* 0 sun, 1 sun and clouds, 2 clouds, 3 rain, 4 snow, 5 storm */
    char city[24];
    struct { int16_t low, high; uint8_t code; } days[3];
    uint8_t valid;
} ow_weather;

typedef struct ow_music {
    char title[48], artist[40];
    uint8_t playing, volume;      /* volume 0..100 */
} ow_music;

typedef struct ow_state {
    owf_values v;                 /* time, date, battery, activity ... (also what the watch face shows) */
    const uint8_t *face;          /* current watch face file */
    uint32_t face_len;
    uint8_t bluetooth_on, flashlight;
    uint8_t notes;                /* number of notifications */
    const char *note_app[3], *note_text[3];
    ow_weather weather;
    ow_music music;
    uint8_t heart_hours[24];      /* average pulse per hour, 0 = no value */
} ow_state;

enum { OW_TILE_ACTIVITY, OW_TILE_FACE, OW_TILE_APPS, OW_TILE_CONTROL, OW_TILE_NOTES };

/* builds the interface on the active screen; `s` must stay valid */
void ow_ui_create(ow_state *s);
ow_state *ow_ui_state(void);
/* the state changed (time, values, toggles): redraw what shows it */
void ow_ui_update(void);
/* the list of apps changed (install, remove) */
void ow_ui_apps_changed(void);
/* jump to a tile (no animation) */
void ow_ui_show(int tile);
/* load the watch face named in the settings from faces/ (falls back to the first face there) */
void ow_ui_load_face(void);

#endif
