/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS hardware test 7: own touch (OWLP version 2, returns to the app).
 *
 * Like hw_solo.c, the vendor firmware is stopped, and now the touch controller is read by our own code too:
 * software I2C on PG03/PG04 (owl_i2c.h), polled every 10 ms (no interrupts while the vendor is stopped).
 *   1. imd_learn() (owl_solo.h), stop the vendor (cpu_suspend_other_core 85), free the I2C lines (9 clocks).
 *   2. Screen "EIGENER TOUCH": a yellow dot where the finger is, at the bottom "X n Y n", "T n" = readings with a
 *      finger, "E n" = readings without an answer from the controller. Right key = clear the dots.
 *      Crown held 2 s = end, 5 minutes at most.
 *   3. Resume the vendor; screen "ZURUECK" (vendor driver, 10 s) with T and E; back to the app.
 */
#include "owl_font.h"
#include "owl_imd.h"
#include "hw_common.h"
#include "owl_solo.h"
#include "owl_i2c.h"

#define DOT 10u
#define TOP 70u
#define BOTTOM 340u

OWL_BLOB(owl_data, OWL_FONT_ASM
         "  .asciz \"EIGENER TOUCH\"\n  .asciz \"HERSTELLER PAUSIERT\"\n  .asciz \"RECHTS = LOESCHEN\"\n"   /* 0-2 */
         "  .asciz \"KRONE 2 S = ENDE\"\n  .asciz \"ZURUECK\"\n");                                         /* 3, 4 */

/* "A n  B n" */
__attribute__((noinline, section(".text.entry")))
static void pair(const unsigned char *data, unsigned y, unsigned a, unsigned va, unsigned b, unsigned vb, unsigned colour) {
    unsigned char line[20];
    unsigned n = 0;
    line[n++] = (unsigned char)a; n += padded(line + n, va, 4u);
    line[n++] = ' '; line[n++] = ' ';
    line[n++] = (unsigned char)b; n += padded(line + n, vb, 4u);
    text(data, OWL_CENTER, y, line, n, 3u, colour, BLACK);
}

__attribute__((noinline, section(".text.entry")))
static void screen(const unsigned char *data) {
    owl_rect(0, 0, OWL_W, OWL_H, BLACK);
    label(data, OWL_CENTER, 10u, 0u, 3u, CYAN, BLACK);
    label(data, OWL_CENTER, 44u, 1u, 2u, WHITE, BLACK);
    label(data, OWL_CENTER, 404u, 2u, 2u, GREY, BLACK);
    label(data, OWL_CENTER, 426u, 3u, 2u, GREY, BLACK);
}

__attribute__((used, section(".text.entry"))) void owl_entry(void) {
    const unsigned char *data = OWL_BLOB_DATA(owl_data);
    unsigned *c = imd_ctx();
    for (unsigned i = 0; i < 20u; i++) c[i] = 0;
    imd_learn();
    OWL_SUSPEND_VENDOR();
    c[CX_OWN] = 1u;
    i2c_recover();
    screen(data);
    unsigned held = owl_keys(), ticks = 0, crown = 0, touches = 0, errors = 0, x = 0, y = 0;
    pair(data, 350u, 'X', 0, 'Y', 0, WHITE);
    pair(data, 378u, 'T', 0, 'E', 0, YELLOW);
    for (;;) {
        OWL_WDT_CLEAR();
        unsigned keys = owl_keys(), pressed = keys & ~held;
        held = keys;
        if (pressed & OWL_KEY_RIGHT) { screen(data); pair(data, 350u, 'X', x, 'Y', y, WHITE); }
        unsigned char b[3];
        unsigned seen = 0;
        if (!touch_read(b)) errors++, seen = 1;
        else if ((b[0] & 3u) == 1u) {
            unsigned nx = ((b[0] << 2) & 0x100u) | b[1], ny = ((b[0] << 1) & 0x100u) | b[2];
            if (nx > OWL_W - 1u) nx = OWL_W - 1u;
            if (ny > OWL_H - 1u) ny = OWL_H - 1u;
            touches++, seen = 1;
            if (nx != x || ny != y) {
                x = nx; y = ny;
                unsigned x0 = x < DOT / 2u ? 0u : x - DOT / 2u, y0 = y < DOT / 2u ? 0u : y - DOT / 2u;
                if (x0 > OWL_W - DOT) x0 = OWL_W - DOT;
                if (y0 < TOP) y0 = TOP;
                if (y0 > BOTTOM - DOT) y0 = BOTTOM - DOT;
                owl_rect(x0, y0, DOT, DOT, YELLOW);
                pair(data, 350u, 'X', x, 'Y', y, WHITE);
            }
        }
        if (seen && (touches + errors) % 10u == 1u) pair(data, 378u, 'T', touches, 'E', errors, YELLOW);
        crown = keys & OWL_KEY_CROWN ? crown + 1u : 0u;
        if (crown >= 200u || ticks >= 30000u) break;   /* 2 s of 10 ms ticks, 5 minutes */
        imd_busy_ms(10u);
        ticks++;
    }
    i2c_recover();
    IMD_CON0 |= 0x40000000u;
    IMD_SPI |= 0x40000000u;
    c[CX_OWN] = 0;
    OWL_RESUME_VENDOR();
    OWL_LCD_CLEAR(0x000000u);
    imd_busy_ms(30u);
    label(data, OWL_CENTER, 80u, 4u, 4u, CYAN, BLACK);
    pair(data, 170u, 'T', touches, 'E', errors, YELLOW);
    pair(data, 230u, 'X', x, 'Y', y, WHITE);
    for (unsigned s = 0; s < 100u; s++) { OWL_WDT_CLEAR(); imd_busy_ms(100u); }   /* 10 s for a photo */
}
