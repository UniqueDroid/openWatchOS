/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS UI 1 (OWLP version 2): first own surface, as start program or started from the app.
 *
 * With the vendor firmware stopped (owl_solo.h), all drawing, touch and keys are our own code:
 *   watch face   date, big HH:MM, seconds; swipe left or right key = menu
 *   menu         STOPPUHR, LAMPE, MALEN, INFO (tap); swipe right or left key = watch face
 *   stopwatch    tap = start/stop, right key = zero
 *   lamp         white screen; tap = back
 *   paint        finger draws, right key = clear
 *   info         L loops per ms, Z seconds running, T touch readings, G gestures (taps and swipes)
 * Everywhere: left key = back, crown short = watch face, crown held 2 s = hand the watch back to the vendor.
 *
 * Time: the RTC is read once before the vendor stops (vendor rtc_now_seconds 0x6003d12, seconds since 2000-01-01,
 * as time() for the JS apps); from then on the own clock (OWL_CLOCK in owl_imd.h, core tick timer) counts.
 */
#define OWL_CLOCK
#include "owl_font.h"
#include "owl_imd.h"
#include "hw_common.h"
#include "owl_solo.h"
#include "owl_i2c.h"

#define RTC_SECONDS ((unsigned (*)(void))0x6003d12u)
#define ORANGE 0xFD20u
#define PURPLE 0xA81Fu

enum { WATCH, MENU, STOPWATCH, LAMP, PAINT, INFO };

OWL_BLOB(owl_data, OWL_FONT_ASM
         "  .asciz \"MO\"\n  .asciz \"DI\"\n  .asciz \"MI\"\n  .asciz \"DO\"\n  .asciz \"FR\"\n  .asciz \"SA\"\n  .asciz \"SO\"\n"  /* 0-6 */
         "  .asciz \"MENUE\"\n  .asciz \"STOPPUHR\"\n  .asciz \"LAMPE\"\n  .asciz \"MALEN\"\n  .asciz \"INFO\"\n"            /* 7-11 */
         "  .asciz \"WISCHEN = MENUE\"\n  .asciz \"TIPPEN = START/STOPP\"\n  .asciz \"RECHTS = NULL\"\n"                /* 12-14 */
         "  .asciz \"RECHTS = LOESCHEN\"\n  .asciz \"LINKS = ZURUECK\"\n  .asciz \"OPENWATCHOS UI 1\"\n"                   /* 15-17 */
         "  .asciz \"UHR: TICK-ZAEHLER\"\n  .asciz \"KRONE 2 S = HERSTELLER\"\n");                                          /* 18, 19 */

/* two digits */
__attribute__((noinline, section(".text.entry")))
static unsigned two(unsigned char *out, unsigned v) {
    out[0] = (unsigned char)('0' + v / 10u % 10u); out[1] = (unsigned char)('0' + v % 10u);
    return 2u;
}

/* "FR 20.11.2026" from seconds since 2000-01-01 (2000..2099: every fourth year a leap year) */
__attribute__((noinline, section(".text.entry")))
static void date_line(const unsigned char *data, unsigned seconds) {
    unsigned days = seconds / 86400u, year = 2000u, month = 0, n;
    const unsigned char *wd = string(data, (days + 5u) % 7u);   /* 2000-01-01 was a Saturday (index 5) */
    for (;;) { n = year % 4u ? 365u : 366u; if (days < n) break; days -= n; year++; }
    for (;;) {
        n = month == 1u ? (year % 4u ? 28u : 29u) : (month == 3u || month == 5u || month == 8u || month == 10u) ? 30u : 31u;
        if (days < n) break;
        days -= n; month++;
    }
    unsigned char line[16];
    unsigned k = 0;
    line[k++] = wd[0]; line[k++] = wd[1]; line[k++] = ' ';
    k += two(line + k, days + 1u); line[k++] = '.';
    k += two(line + k, month + 1u); line[k++] = '.';
    k += two(line + k, year / 100u); k += two(line + k, year);
    text(data, OWL_CENTER, 70u, line, k, 3u, GREY, BLACK);
}

__attribute__((noinline, section(".text.entry")))
static void clock_face(const unsigned char *data, unsigned seconds, unsigned what) {
    unsigned char line[8];
    unsigned rest = seconds % 86400u;
    if (what & 1u) {
        two(line, rest / 3600u); line[2] = ':'; two(line + 3, rest / 60u % 60u);
        text(data, OWL_CENTER, 140u, line, 5u, 10u, WHITE, BLACK);
    }
    if (what & 2u) { two(line, rest % 60u); text(data, OWL_CENTER, 250u, line, 2u, 5u, CYAN, BLACK); }
    if (what & 4u) date_line(data, seconds);
}

__attribute__((noinline, section(".text.entry")))
static void stopwatch_face(const unsigned char *data, unsigned ms) {
    unsigned char line[8];
    unsigned t = ms / 100u;
    two(line, t / 600u); line[2] = ':'; two(line + 3, t / 10u % 60u); line[5] = '.'; line[6] = (unsigned char)('0' + t % 10u);
    text(data, OWL_CENTER, 160u, line, 7u, 7u, ORANGE, BLACK);
}

/* "LABEL n" */
__attribute__((noinline, section(".text.entry")))
static void info_line(const unsigned char *data, unsigned y, unsigned tag, unsigned v) {
    unsigned char line[12];
    line[0] = (unsigned char)tag; line[1] = ' ';
    unsigned n = 2u + padded(line + 2, v, 7u);
    text(data, OWL_CENTER, y, line, n, 3u, WHITE, BLACK);
}

__attribute__((noinline, section(".text.entry")))
static void page(const unsigned char *data, unsigned p) {
    owl_rect(0, 0, OWL_W, OWL_H, p == LAMP ? WHITE : BLACK);
    if (p == WATCH) label(data, OWL_CENTER, 404u, 12u, 2u, GREY, BLACK);
    if (p == MENU) {
        label(data, OWL_CENTER, 20u, 7u, 4u, CYAN, BLACK);
        for (unsigned i = 0; i < 4u; i++) {
            unsigned y = 90u + 80u * i, colour = i == 0u ? ORANGE : i == 1u ? YELLOW : i == 2u ? GREEN : PURPLE;
            owl_rect(20u, y, 328u, 64u, DARK);
            owl_rect(20u, y, 10u, 64u, colour);
            label(data, OWL_CENTER, y + 19u, 8u + i, 3u, WHITE, DARK);
        }
    }
    if (p == STOPWATCH) {
        label(data, OWL_CENTER, 30u, 8u, 4u, ORANGE, BLACK);
        label(data, OWL_CENTER, 360u, 13u, 2u, GREY, BLACK);
        label(data, OWL_CENTER, 386u, 14u, 2u, GREY, BLACK);
    }
    if (p == PAINT) label(data, OWL_CENTER, 410u, 15u, 2u, GREY, BLACK);
    if (p == INFO) {
        label(data, OWL_CENTER, 30u, 17u, 3u, CYAN, BLACK);
        label(data, OWL_CENTER, 80u, 18u, 2u, WHITE, BLACK);
        label(data, OWL_CENTER, 390u, 19u, 2u, GREY, BLACK);
    }
    if (p != WATCH && p != LAMP) label(data, OWL_CENTER, 426u, 16u, 2u, GREY, BLACK);
}

__attribute__((used, section(".text.entry"))) void owl_entry(void) {
    const unsigned char *data = OWL_BLOB_DATA(owl_data);
    unsigned *c = imd_ctx();
    for (unsigned i = 0; i < 20u; i++) c[i] = 0;
    imd_learn();
    unsigned base_sec = RTC_SECONDS();
#ifndef OWL_PREVIEW
    OWL_SUSPEND_VENDOR();
    c[CX_OWN] = 1u;
#endif
    c[CX_CLK_PREV] = TICK_CNT;
    unsigned base_ms = owl_clock();
    i2c_recover();
#ifdef OWL_PREVIEW   /* model only: the vendor driver draws (the model can show that), one page, a few rounds */
    unsigned p = OWL_PREVIEW, rounds = 0;
#else
    unsigned p = WATCH;
#endif
    unsigned shown = ~0u, held = owl_keys(), crown = 0, crown_was = 0;
    unsigned down = 0, sx = 0, sy = 0, lx = 0, ly = 0, lost = 0, touches = 0, gestures = 0;
    unsigned sw_run = 0, sw_start = 0, sw_acc = 0, sw_shown = ~0u;
    page(data, p);
    for (;;) {
        OWL_WDT_CLEAR();
        unsigned now = owl_clock(), next = p;
        if (!TICK_PRD) now = c[CX_CLK_MS] += 10u;   /* no tick timer: count the 10 ms waits */
        /* keys */
        unsigned keys = owl_keys(), pressed = keys & ~held;
        held = keys;
        crown = keys & OWL_KEY_CROWN ? crown + 1u : 0u;
        if (crown >= 200u) break;                                           /* 2 s: back to the vendor */
        if (!crown && crown_was && crown_was < 100u) next = WATCH;          /* short press */
        crown_was = crown;
        if (pressed & OWL_KEY_LEFT) next = p == MENU || p == WATCH ? WATCH : MENU;
        if (pressed & OWL_KEY_RIGHT) {
            if (p == WATCH) next = MENU;
            if (p == STOPWATCH) { sw_run = 0; sw_acc = 0; sw_shown = ~0u; }
            if (p == PAINT) page(data, PAINT);
        }
        /* touch: a finger answers, no finger = no answer */
        unsigned char b[3];
        if (touch_read(b) && (b[0] & 3u) == 1u) {
            unsigned x = ((b[0] << 2) & 0x100u) | b[1], y = ((b[0] << 1) & 0x100u) | b[2];
            if (x > OWL_W - 1u) x = OWL_W - 1u;
            if (y > OWL_H - 1u) y = OWL_H - 1u;
            touches++;
            if (!down) { down = 1; sx = x; sy = y; }
            lx = x; ly = y; lost = 0;
            if (p == PAINT && y > 8u && y < 400u) owl_rect(x < 6u ? 0u : x - 6u, y - 6u, 12u, 12u, YELLOW);
        } else if (down && ++lost >= 3u) {                                  /* finger up: tap or swipe */
            int dx = (int)lx - (int)sx, dy = (int)ly - (int)sy;
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            down = 0; gestures++;
            if (adx > 70 && adx > ady) {
                if (dx < 0 && p == WATCH) next = MENU;
                if (dx > 0 && p == MENU) next = WATCH;
                if (dx > 0 && (p == STOPWATCH || p == INFO)) next = MENU;
            } else if (adx < 30 && ady < 30) {
                if (p == MENU && sy >= 90u && sy < 410u && (sy - 90u) % 80u < 64u) next = STOPWATCH + (sy - 90u) / 80u;
                else if (p == LAMP || p == INFO) next = MENU;
                else if (p == STOPWATCH) {
                    if (sw_run) { sw_acc += now - sw_start; sw_run = 0; } else { sw_start = now; sw_run = 1; }
                }
            }
        }
        /* page change, then what changes on the page */
        if (next != p) { p = next; page(data, p); shown = ~0u; sw_shown = ~0u; }
        if (p == WATCH) {
            unsigned sec = base_sec + (now - base_ms) / 1000u;
            if (sec != shown) {
                clock_face(data, sec, shown == ~0u ? 7u : sec / 60u != shown / 60u ? 3u : 2u);
                if (sec / 86400u != shown / 86400u && shown != ~0u) clock_face(data, sec, 4u);
                shown = sec;
            }
        }
        if (p == STOPWATCH) {
            unsigned ms = sw_acc + (sw_run ? now - sw_start : 0u);
            if (ms / 100u != sw_shown) { stopwatch_face(data, ms); sw_shown = ms / 100u; }
        }
        if (p == INFO && (now - base_ms) / 1000u != shown) {
            shown = (now - base_ms) / 1000u;
            info_line(data, 140u, 'L', c[CX_BLOCKS]);
            info_line(data, 190u, 'Z', shown);
            info_line(data, 240u, 'T', touches);
            info_line(data, 290u, 'G', gestures);
        }
        imd_busy_ms(10u);
#ifdef OWL_PREVIEW
        if (++rounds >= 3u) return;
#endif
    }
    i2c_recover();
    IMD_CON0 |= 0x40000000u;
    IMD_SPI |= 0x40000000u;
    c[CX_OWN] = 0;
    OWL_RESUME_VENDOR();
}
