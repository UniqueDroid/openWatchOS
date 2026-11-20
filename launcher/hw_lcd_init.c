/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS hardware test 8: start the display ourselves (OWLP version 2, returns to the app).
 *
 * The own driver so far took over a display the vendor had switched on. A firmware of our own must switch it on
 * itself after a restart. This test, with the vendor stopped (owl_solo.h):
 *   1. Screen "DISPLAY NEUSTART" (own driver), 3 s.
 *   2. Display off: 0x28 (display off), 0x10 (sleep in). 2 s dark.
 *   3. Hardware reset of the panel: PA02 low 20 ms, high, 150 ms (reset pin from the board table, HARDWARE-M78.md).
 *   4. Own init: the vendor's power-on table for the GP3112 (vendor 0x88bfc;
 *      it is data for the panel), then brightness 0x51 = 0xFF (the vendor sets brightness separately).
 *   5. Screen "NEU EINGESCHALTET" with colour bars and seconds. Crown held 2 s = end, 5 minutes at most.
 *   6. Resume the vendor; screen "ZURUECK" (vendor driver, 10 s); back to the app.
 * If step 4 fails the screen stays dark: hold the crown 2 s anyway; the vendor comes back (its screen may stay dark
 * until the display is switched off and on with a key, or the crown is held 8 s for a restart).
 */
#include "owl_font.h"
#include "owl_imd.h"
#include "hw_common.h"
#include "owl_solo.h"

#define PA_RESET (1u << 2)
#define DELAY 0xFFu   /* table: DELAY, ms */

OWL_BLOB(owl_data, OWL_FONT_ASM
         "  .asciz \"DISPLAY NEUSTART\"\n  .asciz \"GLEICH WIRD ES DUNKEL\"\n"                         /* 0, 1 */
         "  .asciz \"NEU EINGESCHALTET\"\n  .asciz \"VOM EIGENEN CODE\"\n"                             /* 2, 3 */
         "  .asciz \"KRONE 2 S = ENDE\"\n  .asciz \"ZURUECK\"\n");                                     /* 4, 5 */

/* command, count, bytes ... | DELAY, ms | 0 = end */
OWL_BLOB(owl_init,
         "  .byte 0x11, 0\n  .byte 0xFF, 100\n"
         "  .byte 0xC0, 2, 0x5A, 0x5A\n  .byte 0xB0, 1, 0x08\n  .byte 0xB1, 2, 0xF0, 0x00\n  .byte 0xC0, 2, 0xA5, 0xA5\n"
         "  .byte 0x2A, 5, 0x00, 0x00, 0x00, 0x01, 0x6F\n  .byte 0x2B, 5, 0x00, 0x00, 0x00, 0x01, 0xBF\n"
         "  .byte 0x44, 2, 0x00, 0x0A\n  .byte 0x35, 1, 0x00\n  .byte 0x4A, 1, 0xFF\n  .byte 0x3A, 1, 0x55\n"
         "  .byte 0x53, 1, 0x20\n  .byte 0xFF, 25\n  .byte 0x29, 0\n  .byte 0x51, 1, 0xFF\n  .byte 0\n");

__attribute__((noinline, section(".text.entry")))
static void run_table(const unsigned char *t) {
    while (*t) {
        if (*t == DELAY) { imd_busy_ms(t[1]); t += 2; continue; }
        imd_write_cmd(t[0], t + 2, t[1]);
        t += 2u + t[1];
    }
}

__attribute__((noinline, section(".text.entry")))
static void seconds(const unsigned char *data, unsigned v) {
    unsigned char line[8];
    line[0] = 'Z';
    unsigned n = 1u + padded(line + 1, v, 4u);
    text(data, OWL_CENTER, 250u, line, n, 4u, GREEN, BLACK);
}

__attribute__((used, section(".text.entry"))) void owl_entry(void) {
    const unsigned char *data = OWL_BLOB_DATA(owl_data);
    unsigned *c = imd_ctx();
    for (unsigned i = 0; i < 20u; i++) c[i] = 0;
    imd_learn();
    OWL_SUSPEND_VENDOR();
    c[CX_OWN] = 1u;
    /* 1 */
    owl_rect(0, 0, OWL_W, OWL_H, BLACK);
    label(data, OWL_CENTER, 150u, 0u, 3u, CYAN, BLACK);
    label(data, OWL_CENTER, 210u, 1u, 2u, WHITE, BLACK);
    for (unsigned i = 0; i < 30u; i++) { OWL_WDT_CLEAR(); imd_busy_ms(100u); }
    /* 2 */
    imd_write_cmd(0x28u, 0, 0);
    imd_busy_ms(20u);
    imd_write_cmd(0x10u, 0, 0);
    for (unsigned i = 0; i < 20u; i++) { OWL_WDT_CLEAR(); imd_busy_ms(100u); }
    /* 3 */
    PA_OUT &= ~PA_RESET;
    imd_busy_ms(20u);
    PA_OUT |= PA_RESET;
    imd_busy_ms(150u);
    OWL_WDT_CLEAR();
    /* 4 */
    run_table(OWL_BLOB_DATA(owl_init));
    OWL_WDT_CLEAR();
    /* 5 */
    owl_rect(0, 0, OWL_W, OWL_H, BLACK);
    label(data, OWL_CENTER, 40u, 2u, 3u, CYAN, BLACK);
    label(data, OWL_CENTER, 80u, 3u, 2u, WHITE, BLACK);
    owl_rect(44u, 130u, 94u, 60u, RED);
    owl_rect(138u, 130u, 94u, 60u, GREEN);
    owl_rect(232u, 130u, 94u, 60u, BLUE);
    label(data, OWL_CENTER, 400u, 4u, 2u, GREY, BLACK);
    unsigned ticks = 0, crown = 0, shown = ~0u;
    for (;;) {
        OWL_WDT_CLEAR();
        if (ticks / 100u != shown) seconds(data, shown = ticks / 100u);
        crown = owl_keys() & OWL_KEY_CROWN ? crown + 1u : 0u;
        if (crown >= 200u || ticks >= 30000u) break;
        imd_busy_ms(10u);
        ticks++;
    }
    /* 6 */
    IMD_CON0 |= 0x40000000u;
    IMD_SPI |= 0x40000000u;
    c[CX_OWN] = 0;
    OWL_RESUME_VENDOR();
    OWL_LCD_CLEAR(0x000000u);
    imd_busy_ms(30u);
    label(data, OWL_CENTER, 120u, 5u, 4u, CYAN, BLACK);
    seconds(data, ticks / 100u);
    for (unsigned s = 0; s < 100u; s++) { OWL_WDT_CLEAR(); imd_busy_ms(100u); }
}
