/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_LCD_H
#define OWL_LCD_H
/* openWatchOS launcher: drawing through the vendor LCD driver (4.50.x, found with rueckweg 1.4.x/1.5.0, see
 * docs/firmware-interface.md). Macros only: an OWLP program is one position-independent function.
 *
 * lcd_interface table at 0x6130a50 (order as struct lcd_interface in the umeox SDK):
 *   draw 0x6022c76 (buf, len, wait)   starts the transfer and does NOT wait for it -> OWL_GAP after each window
 *   set_draw_area 0x6022c86 (xs, xe, ys, ye)
 *   clear_screen 0x6022cda (0xRRGGBB) fills the whole screen in hardware, no buffer
 * Pixels are RGB565 with the HIGH byte first in memory (1.5.0 on the watch: 0xF800 stored little-endian came out
 * blue, 0x07E0 red, 0x001F green), so OWL_PIX swaps the two bytes.
 * Buffer: the driver's own draw buffer (pointer [cfg+40], size [cfg+36], cfg = [0x106430], set by buffer_malloc
 * 0x6022c00). The display hardware reads it for sure, and nothing is allocated: 1.4.3/1.5.0 took a new PSRAM block
 * per run and never gave it back, after a few runs the PSRAM draws stayed away until a restart.
 * Include owl_sys.h first (owl_rect waits with owl_wait_ms).
 * The program runs in the GUI task, so the GUI does not touch that buffer meanwhile and redraws after the return.
 */
typedef void (*owl_flush_fn)(void *, int);
typedef void (*owl_clear_fn)(unsigned);
typedef void (*owl_area_fn)(unsigned, unsigned, unsigned, unsigned);
typedef void (*owl_draw_fn)(unsigned char *, unsigned, unsigned);

#define OWL_FLUSH_DCACHE ((owl_flush_fn)0x1fdacu)
#define OWL_VENDOR_DRAW ((owl_draw_fn)0x6022c76u)
#define OWL_VENDOR_SET_AREA ((owl_area_fn)0x6022c86u)
#ifndef OWL_LCD_DRAW   /* a program can draw through its own driver instead (owl_imd.h) */
#define OWL_LCD_DRAW OWL_VENDOR_DRAW
#define OWL_LCD_SET_AREA OWL_VENDOR_SET_AREA
#endif
#define OWL_LCD_CLEAR ((owl_clear_fn)0x6022cdau)
#define OWL_LCD_CFG (*(unsigned char *volatile *)0x106430u)
#define OWL_LCD_BUF ((unsigned short *)*(unsigned *)(OWL_LCD_CFG + 40))
#define OWL_LCD_BUF_SIZE (*(unsigned *)(OWL_LCD_CFG + 36))
#define OWL_W 368
#define OWL_H 448
#define OWL_PIX(c) ((unsigned short)((((c) & 0xFFu) << 8) | (((c) >> 8) & 0xFFu)))
/* The panel (GP3112) needs windows at an even column with an even width: hwtest 1.1.0 drew text of scale 3
 * (odd width and height) slanted (Jan, 20.11.2026). owl_rect and text() use even rows, columns and sizes. */
#define OWL_GAP_MS 10u   /* draw only starts the transfer: wait this long before the next window */

/* rectangle (x, y, w, h) in one RGB565 colour, in as many windows as the driver buffer needs. Needs owl_sys.h. */
__attribute__((noinline, unused, section(".text.entry")))
static void owl_rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned colour) {
    w += x & 1u; x &= ~1u; w = (w + 1u) & ~1u;   /* even column, row and sizes, see text() in owl_text.h */
    h += y & 1u; y &= ~1u; h = (h + 1u) & ~1u;
    unsigned short *b = OWL_LCD_BUF;
    unsigned rows = b ? OWL_LCD_BUF_SIZE / (w * 2u) & ~1u : 0u;
    if (!rows) return;
    if (rows > h) rows = h;
    for (unsigned k = 0; k < w * rows; k++) b[k] = OWL_PIX(colour);
    OWL_FLUSH_DCACHE(b, w * rows * 2u);
    for (unsigned top = 0; top < h; top += rows) {
        unsigned n = h - top < rows ? h - top : rows;
        OWL_LCD_SET_AREA(x, x + w - 1u, y + top, y + top + n - 1u);
        OWL_LCD_DRAW((unsigned char *)b, w * n * 2u, 1);
        owl_wait_ms(OWL_GAP_MS);
    }
}
#endif
