/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_UI_H
#define OW_UI_H
/* openWatchOS user interface on LVGL 8.3 (the version of the JieLi watch SDK).
 *
 * Layout (swipe like the stock firmware):
 *                 control centre
 *                       ^ (swipe down from the face)
 *   activity  <-   watch face   ->  apps
 *                       v (swipe up)
 *                 notifications
 * The watch face is a stock watch face file ("JL" format, fw/owf), drawn directly into LVGL's draw buffer.
 */
#include "lvgl.h"
#include "../owf/owf.h"

typedef struct ow_state {
    owf_values v;                 /* time, date, battery, activity ... (also what the watch face shows) */
    const uint8_t *face;          /* current watch face file */
    uint32_t face_len;
    uint8_t bluetooth_on, dnd, aod, flashlight, brightness;   /* brightness 0..100 */
    uint8_t notes;                /* number of notifications */
    const char *note_app[3], *note_text[3];
} ow_state;

enum { OW_TILE_ACTIVITY, OW_TILE_FACE, OW_TILE_APPS, OW_TILE_CONTROL, OW_TILE_NOTES };

/* builds the interface on the active screen; `s` must stay valid */
void ow_ui_create(ow_state *s);
/* the state changed (time, values, toggles): redraw what shows it */
void ow_ui_update(void);
/* jump to a tile (no animation) */
void ow_ui_show(int tile);

#endif
