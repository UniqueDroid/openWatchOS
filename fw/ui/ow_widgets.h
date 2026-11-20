/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_WIDGETS_H
#define OW_WIDGETS_H
/* Shared look of openWatchOS: fonts, colours, round buttons, list rows, titles. Everything on black (AMOLED). */
#include "lvgl.h"
#include "ow_icons.h"

LV_FONT_DECLARE(ow_font_20)
LV_FONT_DECLARE(ow_font_28)
LV_FONT_DECLARE(ow_font_40)
LV_FONT_DECLARE(ow_font_64)
LV_FONT_DECLARE(ow_icon_56)

#define OW_W 368
#define OW_H 448
#define OW_GREY 0x98989D
#define OW_CARD 0x1C1C1E
#define OW_CARD_PRESSED 0x2C2C2E
#define OW_BLUE 0x0A84FF
#define OW_GREEN 0x30D158
#define OW_RED 0xFF375F
#define OW_ORANGE 0xFF9F0A
#define OW_YELLOW 0xFFD60A
#define OW_PURPLE 0xBF5AF2

lv_obj_t *ow_label(lv_obj_t *parent, const lv_font_t *font, uint32_t colour, const char *text);
lv_obj_t *ow_round_icon(lv_obj_t *parent, int size, uint32_t colour, const char *symbol);
/* full-screen page with a title at the top; returns the content area below it (scrolls vertically) */
lv_obj_t *ow_page(lv_obj_t *screen, const char *title, uint32_t title_colour);
/* rounded list row with a round icon and a text; `right` (may be NULL) is shown at the right edge */
lv_obj_t *ow_row(lv_obj_t *list, const char *symbol, uint32_t colour, const char *text, const char *right);
/* round button with a symbol */
lv_obj_t *ow_button(lv_obj_t *parent, int size, uint32_t colour, const char *symbol, lv_event_cb_t cb, void *user);
/* switch styled for the watch */
lv_obj_t *ow_switch(lv_obj_t *parent, int on, lv_event_cb_t cb, void *user);

#endif
