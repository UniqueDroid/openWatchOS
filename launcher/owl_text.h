/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_TEXT_H
#define OWL_TEXT_H
/* openWatchOS launcher: text output for OWLP programs (rueckweg 1.6.0, checked on the watch 19.11.2026).
 * Needs owl_lcd.h and owl_font.h before it. The program's OWL_BLOB starts with OWL_FONT_ASM; strings may follow
 * (string(data, n) finds the n-th). text() draws one line in a box just as wide as the text. Include order:
 * owl_sys.h, owl_lcd.h, owl_font.h, owl_text.h.
 */
__attribute__((noinline, unused, section(".text.entry")))
static const unsigned char *string(const unsigned char *data, unsigned n) {
    const unsigned char *s = data + OWL_FONT_SIZE;
    while (n--) { while (*s) s++; s++; }
    return s;
}

#define OWL_CENTER 0xFFFFu

/* one line of text at x (or OWL_CENTER), in a box of 9 x scale rows from y on (one empty font row above and below),
 * only as wide as the text: shorter text later does not cover longer text, pad with spaces. */
__attribute__((noinline, unused, section(".text.entry")))
static void text(const unsigned char *font, unsigned x, unsigned y, const unsigned char *s, unsigned len,
                 unsigned scale, unsigned fg, unsigned bg) {
    unsigned short *buf = OWL_LCD_BUF;
    if (!buf || !len) return;
    unsigned h = (9u * scale + 1u) & ~1u, w = (len * 6u * scale + scale + 1u) & ~1u;   /* margin; both even */
    if (w > OWL_W) w = OWL_W;
    unsigned x0 = x == OWL_CENTER ? (OWL_W - w) / 2u : x;
    if (x0 + w > OWL_W) x0 = OWL_W - w;
    x0 &= ~1u; y &= ~1u;   /* windows at even rows/columns, even sizes (odd ones came out slanted) */
    unsigned per = OWL_LCD_BUF_SIZE / (w * 2u) & ~1u;   /* rows per window, even */
    if (!per) return;
    unsigned short f = OWL_PIX(fg), b = OWL_PIX(bg);
    for (unsigned top = 0; top < h; top += per) {
        unsigned n = h - top < per ? h - top : per;
        for (unsigned k = 0; k < w * n; k++) buf[k] = b;
        for (unsigned r = 0; r < n; r++) {
            unsigned gy = (top + r) / scale;
            if (gy < 1u || gy > 7u) continue;
            gy -= 1u;
            unsigned short *row = buf + r * w;
            for (unsigned i = 0; i < len; i++) {
                unsigned c = s[i];
                if (c >= 'a' && c <= 'z') c -= 32u;
                if (c < OWL_FONT_FIRST || c > OWL_FONT_LAST) c = '?';
                const unsigned char *glyph = font + (c - OWL_FONT_FIRST) * 5u;
                for (unsigned col = 0; col < 5u; col++) {
                    if (!((glyph[col] >> gy) & 1u)) continue;
                    unsigned px = scale + (i * 6u + col) * scale;
                    for (unsigned d = 0; d < scale && px + d < w; d++) row[px + d] = f;
                }
            }
        }
        OWL_FLUSH_DCACHE(buf, w * n * 2u);
        OWL_LCD_SET_AREA(x0, x0 + w - 1u, y + top, y + top + n - 1u);
        OWL_LCD_DRAW((unsigned char *)buf, w * n * 2u, 1);
        owl_wait_ms(OWL_GAP_MS);
    }
}

__attribute__((noinline, unused, section(".text.entry")))
static unsigned length(const unsigned char *s) {
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}

/* number to decimal digits at out, returns the count */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned decimal(unsigned char *out, unsigned value) {
    unsigned char digits[10];
    unsigned d = 0, n = 0;
    do { digits[d++] = (unsigned char)('0' + value % 10u); value /= 10u; } while (value && d < 10u);
    while (d) out[n++] = digits[--d];
    return n;
}

/* the n-th string of the blob, at x (or OWL_CENTER) */
__attribute__((noinline, unused, section(".text.entry")))
static void label(const unsigned char *data, unsigned x, unsigned y, unsigned n, unsigned scale, unsigned fg,
                  unsigned bg) {
    const unsigned char *s = string(data, n);
    text(data, x, y, s, length(s), scale, fg, bg);
}

/* number right-aligned in `width` characters (spaces in front) */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned padded(unsigned char *out, unsigned value, unsigned width) {
    unsigned char digits[12];
    unsigned d = decimal(digits, value), n = 0;
    while (n + d < width) out[n++] = ' ';
    for (unsigned i = 0; i < d; i++) out[n++] = digits[i];
    return n;
}
#endif
