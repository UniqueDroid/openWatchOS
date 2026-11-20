/* SPDX-License-Identifier: GPL-2.0-only */
/* LVGL 9.6 settings of openWatchOS (host simulator and firmware). Everything not set here keeps LVGL's default.
 * Rendering in RGB565; the display flush swaps to high byte first for the M78 panel. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_MEM_SIZE (512 * 1024U)
#define LV_DPI_DEF 130
#define LV_DEF_REFR_PERIOD 20
#define LV_FONT_MONTSERRAT_14 1
#define LV_USE_LOG 0

#endif
