/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWF_H_INCLUDED
#define OWF_H_INCLUDED
/* openWatchOS watch face engine: draws the watch face files of the M78 stock firmware ("JL" files, 24-byte elements,
 * see docs/watchface-format.md) into a band of the screen. No allocation, no globals, no floating point: the same
 * code runs on the host (tests, previews) and in the firmware (LVGL draw buffer of 368 x n rows).
 */
#include <stdint.h>

#define OWF_W 368
#define OWF_H 448
#define OWF_MAX_ELEMENTS 88   /* the stock firmware reads at most 88 */

typedef struct owf_values {
    uint8_t hour, minute, second;           /* 24-hour clock */
    uint8_t day, month, weekday;            /* day 1..31, month 1..12, weekday 0 = Monday */
    uint16_t year;
    uint8_t battery, bluetooth, unread, weather, lang, h12, style;
    int16_t temperature, compass;
    uint32_t steps, kcal, heart, distance_m, sleep_min, step_goal, kcal_goal, spo2;
    uint32_t ms;                             /* running time, selects animation frames */
} owf_values;

/* A band of the screen: rows y0 .. y0 + rows - 1, full width, RGB565. swap = 1 stores the high byte first
 * (the M78 panel), 0 stores native little-endian values. */
typedef struct owf_band {
    uint16_t *px;
    int y0, rows;
    int swap;
} owf_band;

/* 0 = the file looks like a watch face (header, element table and image offsets inside the file) */
int owf_check(const uint8_t *file, uint32_t len);

/* Draws all elements that touch the band. Returns the number of elements that were not drawn (unknown type or
 * undecodable image); `skipped`, if given, gets one bit per element index < 64. */
int owf_render(const uint8_t *file, uint32_t len, const owf_values *v, const owf_band *band, uint64_t *skipped);

/* Tap action of the first tap area (type 0xb4) containing (x, y), or 0. */
int owf_tap(const uint8_t *file, uint32_t len, int x, int y);

/* Shortest redraw period the elements ask for, in ms (500 with seconds, 1000 otherwise, 60000 without either). */
uint32_t owf_period(const uint8_t *file, uint32_t len);

#endif
