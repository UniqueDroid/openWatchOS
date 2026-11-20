/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS hardware test 6: on its own (OWLP version 2, returns to the app).
 *
 * First program that runs the watch without the vendor firmware for a while:
 *   1. Watch the vendor driver draw one window and keep its settings (owl_imd.h, imd_learn below), measure how
 *      many busy loops make a millisecond. Show what was found (screen 1; right key or 30 s goes on).
 *   2. Stop the vendor firmware: cpu_suspend_other_core(85) (vendor 0x11f76e; parks the second core and turns the
 *      interrupts of this one off, as the firmware does when writing flash). From here on nothing of the vendor
 *      runs: no tasks, no interrupts, no clock.
 *   3. Own display driver (owl_imd.h), keys straight from the ports: screen 2 with colour bars, a seconds count
 *      (busy loops), counters for the side keys. Crown held 2 s = end, 5 minutes at most.
 *   4. cpu_resume_other_core(85) (0x11f84c); screen 3 by the vendor driver for 10 s: "ZURUECK" with the seconds,
 *      key counts and failed waits of the own phase (shows that it ran, even if its screen stayed dark); back.
 * owl_wait_ms() is switched off here (OWL_BUSY_WAIT in owl_imd.h); waits use imd_busy_ms(), calibrated in step 1.
 * The watchdog is still cleared (vendor wdt_clear, a register write). If the own driver fails, screen 1 stays,
 * "F n" on screen 2 counts the waits that ran out; after the crown the watch is back as before.
 */
#include "owl_font.h"
#include "owl_imd.h"
#include "hw_common.h"
#include "owl_solo.h"

#define BAR 40u

OWL_BLOB(owl_data, OWL_FONT_ASM
         "  .asciz \"GELERNT\"\n  .asciz \"EIGENER TREIBER\"\n  .asciz \"HERSTELLER PAUSIERT\"\n"   /* 0-2 */
         "  .asciz \"KRONE 2 S = ENDE\"\n  .asciz \"RECHTS = WEITER\"\n  .asciz \"ZURUECK\"\n");     /* 3-5 */

/* "X hhhhhhhh" */
__attribute__((noinline, section(".text.entry")))
static void hexline(const unsigned char *data, unsigned y, unsigned tag, unsigned v) {
    unsigned char line[12];
    line[0] = (unsigned char)tag; line[1] = ' ';
    for (unsigned i = 0; i < 8u; i++) {
        unsigned d = (v >> (28u - 4u * i)) & 15u;
        line[2 + i] = (unsigned char)(d < 10u ? '0' + d : 'A' + d - 10u);
    }
    text(data, OWL_CENTER, y, line, 10u, 3u, WHITE, BLACK);
}

/* "LABEL n" with a number padded to width */
__attribute__((noinline, section(".text.entry")))
static void numline(const unsigned char *data, unsigned x, unsigned y, unsigned tag, unsigned v, unsigned colour) {
    unsigned char line[12];
    line[0] = (unsigned char)tag; line[1] = ' ';
    unsigned n = 2u + padded(line + 2, v, 5u);
    text(data, x, y, line, n, 3u, colour, BLACK);
}

__attribute__((used, section(".text.entry"))) void owl_entry(void) {
    const unsigned char *data = OWL_BLOB_DATA(owl_data);
    unsigned *c = imd_ctx();
    for (unsigned i = 0; i < 20u; i++) c[i] = 0;
    imd_learn();
    /* screen 1 (vendor driver): what was learnt */
    OWL_LCD_CLEAR(0x000000u);
    imd_busy_ms(30u);
    label(data, OWL_CENTER, 14u, 0u, 4u, CYAN, BLACK);
    hexline(data, 70u, 'A', c[CX_AREA_CON]);
    hexline(data, 110u, 'B', c[CX_AREA_BUF]);
    hexline(data, 150u, 'D', c[CX_DRAW_CON]);
    hexline(data, 190u, '1', c[CX_CON1]);
    hexline(data, 230u, 'P', c[CX_PA_LOW]);
    hexline(data, 270u, 'Q', c[CX_PA_HIGH]);
    hexline(data, 310u, 'C', c[CX_CS]);
    numline(data, OWL_CENTER, 350u, 'L', c[CX_BLOCKS], GREEN);
    label(data, OWL_CENTER, 400u, 4u, 2u, GREY, BLACK);
    for (unsigned s = 0, held = owl_keys(); s < 300u; s++) {   /* 30 s for a photo, right key goes on earlier */
        OWL_WDT_CLEAR(); imd_busy_ms(100u);
        unsigned keys = owl_keys();
        if (keys & ~held & OWL_KEY_RIGHT) break;
        held = keys;
    }
    /* step 2: the vendor firmware stops */
    OWL_SUSPEND_VENDOR();
    c[CX_OWN] = 1u;
    /* screen 2 (own driver) */
    owl_rect(0, 0, OWL_W, OWL_H, BLACK);
    label(data, OWL_CENTER, 20u, 1u, 3u, CYAN, BLACK);
    label(data, OWL_CENTER, 60u, 2u, 2u, WHITE, BLACK);
    owl_rect(44u, 100u, 94u, BAR, RED);
    owl_rect(44u + 94u, 100u, 94u, BAR, GREEN);
    owl_rect(44u + 188u, 100u, 94u, BAR, BLUE);
    label(data, OWL_CENTER, 400u, 3u, 2u, GREY, BLACK);
    unsigned held = owl_keys(), left = 0, right = 0, ticks = 0, crown = 0, shown = ~0u, fails = ~0u;
    numline(data, OWL_CENTER, 170u, 'Z', 0, GREEN);
    numline(data, 30u, 240u, 'L', 0, YELLOW);
    numline(data, 200u, 240u, 'R', 0, YELLOW);
    for (;;) {
        OWL_WDT_CLEAR();
        unsigned keys = owl_keys(), pressed = keys & ~held;
        held = keys;
        if (pressed & OWL_KEY_LEFT) numline(data, 30u, 240u, 'L', ++left, YELLOW);
        if (pressed & OWL_KEY_RIGHT) numline(data, 200u, 240u, 'R', ++right, YELLOW);
        if (ticks / 100u != shown) numline(data, OWL_CENTER, 170u, 'Z', shown = ticks / 100u, GREEN);
        if (c[CX_TIMEOUTS] != fails) numline(data, OWL_CENTER, 310u, 'F', fails = c[CX_TIMEOUTS], RED);
        crown = keys & OWL_KEY_CROWN ? crown + 1u : 0u;
        if (crown >= 200u || ticks >= 30000u) break;   /* 2 s of 10 ms ticks, 5 minutes */
        imd_busy_ms(10u);
        ticks++;
    }
    /* step 4: back to the vendor firmware */
    IMD_CON0 |= 0x40000000u;   /* no stale "done" for the vendor's interrupt */
    IMD_SPI |= 0x40000000u;
    c[CX_OWN] = 0;
    OWL_RESUME_VENDOR();
    /* screen 3 (vendor driver again): proof that the own phase ran, even if its screen did not show */
    OWL_LCD_CLEAR(0x000000u);
    imd_busy_ms(30u);
    label(data, OWL_CENTER, 60u, 5u, 4u, CYAN, BLACK);
    numline(data, OWL_CENTER, 140u, 'Z', ticks / 100u, GREEN);
    numline(data, OWL_CENTER, 190u, 'L', left, YELLOW);
    numline(data, OWL_CENTER, 240u, 'R', right, YELLOW);
    numline(data, OWL_CENTER, 290u, 'F', c[CX_TIMEOUTS], RED);
    for (unsigned s = 0; s < 100u; s++) { OWL_WDT_CLEAR(); imd_busy_ms(100u); }   /* 10 s for a photo */
}
