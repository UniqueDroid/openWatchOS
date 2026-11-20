/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_SYS_H
#define OW_SYS_H
/* openWatchOS platform layer: what the interface and the apps need from the hardware and the OS.
 * One implementation per platform: sim/ow_sys_sim.c (PC, files in a folder), later the watch (SDK, private).
 * Paths are relative to the watch's storage root, e.g. "apps/de.jan.hallo/app.js", "faces/skynet.di".
 */
#include <stdint.h>

uint32_t ow_sys_ms(void);                  /* monotonic milliseconds */
uint32_t ow_sys_seconds(void);             /* local time, seconds since 2000-01-01 00:00 */
void ow_sys_set_seconds(uint32_t s);       /* from the phone */
void ow_sys_vibrate(unsigned ms);
void ow_sys_brightness(unsigned percent);  /* display, 0..100 */
unsigned ow_sys_battery(void);             /* percent */

/* storage: whole small files */
int ow_sys_read(const char *path, void *buf, unsigned cap);          /* bytes read, -1 = missing */
int ow_sys_size(const char *path);                                  /* bytes, -1 = missing */
int ow_sys_write(const char *path, const void *buf, unsigned len);  /* 0 = written; creates folders */
int ow_sys_remove(const char *path);                                /* file or folder with its files */
/* calls fn for each entry of a folder (names only, no "." and ".."), returns the number of entries */
int ow_sys_list(const char *dir, void (*fn)(const char *name, int is_dir, void *user), void *user);

/* phone link: short text messages on a channel ("store", "music", "app:<id>", ...) */
int ow_sys_send(const char *channel, const char *text, unsigned len);  /* 1 = handed over */
int ow_sys_connected(void);

/* time helpers (no platform needed) */
typedef struct ow_time { uint16_t year; uint8_t month, day, hour, minute, second, weekday; } ow_time;   /* weekday 0 = Monday */
void ow_time_split(uint32_t seconds, ow_time *t);

#endif
