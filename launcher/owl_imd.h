/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_IMD_H
#define OWL_IMD_H
/* openWatchOS launcher: own display driver, without the vendor LCD driver (hwtest 6 "Eigenstaendig").
 *
 * Registers of the display unit IMD (base 0xfe2d00, struct JL_IMD_TypeDef in the SDK header br28.h):
 *   CON0 +0x00  bit 1 start, bit 13 fill with one colour, bit 29 interrupt on, bits 30/31 clear/pending,
 *               bits 2..11 window height
 *   CON1 +0x04  window width (bits 0..9), height (10..19), bytes per row (20..31)
 *   CON3 +0x0c  window height (bits 0..9)
 *   LD   +0x10  address of the pixels
 *   SPI  +0x20  SPI control: bit 1 start, bit 8, bit 14, bits 9..13 format, bits 15/16 four lines, bit 22 data
 *               (not command), bit 24 pixels from LD, bits 25..27 byte count - 1, bit 29 interrupt on,
 *               bit 30 clear, bit 31 done
 *   BUF  +0x24  word to send
 * The sequence follows what the SDK's IMD library does for a QSPI panel (ui_new.a, imd.c/imd_spi.c, read as
 * LLVM IR: set_buf_size, lcd_spi_set_draw_area, lcd_spi_draw, imd_spi_tx_cmd/dat). Own code, no copy.
 * Bytes per row are rounded up to 4 by the hardware setup ((2w + 3) & ~3): that is why odd widths came out slanted.
 *
 * imd_learn() (hw_solo.c) watches one window drawn by the vendor driver and keeps the pixel format bits and the
 * chip select pin; the command headers are fixed (see IMD_HDR_WRITE).
 * Chip select is a GPIO (PA06 on the M78), found as the bit of PA OUT that goes low during the window.
 * All state sits in a writable block inside the program itself (OWL_BLOB, no globals in an OWLP program).
 */
#define IMD_CON0 (*(volatile unsigned *)0xfe2d00u)
#define IMD_CON1 (*(volatile unsigned *)0xfe2d04u)
#define IMD_CON3 (*(volatile unsigned *)0xfe2d0cu)
#define IMD_LD (*(volatile unsigned *)0xfe2d10u)
#define IMD_SPI (*(volatile unsigned *)0xfe2d20u)
#define IMD_BUF (*(volatile unsigned *)0xfe2d24u)
#define PA_OUT (*(volatile unsigned *)0xfd5000u)

/* context words */
#define CX_AREA_CON 0     /* SPI control right after the vendor's set_draw_area */
#define CX_AREA_BUF 1     /* IMD_BUF afterwards (reads back shifted: 0x002C0000 = 0x32002C00 << 8), for the screen */
#define CX_DRAW_CON 2     /* SPI control right after the vendor started a draw */
#define CX_CS 3           /* chip select bit in PA OUT */
#define CX_BLOCKS 4       /* busy loop blocks (1000 loops) per millisecond */
#define CX_TIMEOUTS 5     /* waits that ran out */
#define CX_OWN 6          /* 1: draw with the own driver */
#define CX_PA_LOW 7       /* PA OUT during the vendor window (for the screen) */
#define CX_PA_HIGH 8      /* PA OUT after it */
#define CX_CON1 9         /* CON1 after the vendor's window (check of the formula) */
#define CX_CLK_PREV 10    /* own clock (OWL_CLOCK): last TICK_CNT */
#define CX_CLK_ACC 11     /*   counts not yet a whole millisecond */
#define CX_CLK_MS 12      /*   milliseconds */

/* Own clock for programs that run with the vendor stopped (no interrupts, so no jiffies). The core tick timer keeps
 * counting: TICK_CNT 0xffe0f0 runs up to TICK_PRD 0xffe0f4 and starts again, one period = 10 ms (SDK
 * jiffies_half_msec: CNT * 20 / PRD + jiffies * 20; tick_timer_init in os_cpu_c.c). owl_clock() adds what passed
 * since the last call; it must come at least every 10 ms, so the waits and the drawing call it. */
#define TICK_CNT (*(volatile unsigned *)0xffe0f0u)
#define TICK_PRD (*(volatile unsigned *)0xffe0f4u)
#ifdef OWL_CLOCK
#define OWL_CLOCK_HOOK() owl_clock()
#else
#define OWL_CLOCK_HOOK() ((void)0)
#endif

OWL_BLOB(owl_imd_blob, "  .space 80\n");

__attribute__((noinline, unused, section(".text.entry")))
static unsigned *imd_ctx(void) {
    return (unsigned *)(((unsigned)OWL_BLOB_DATA(owl_imd_blob) + 3u) & ~3u);
}

/* milliseconds of the own clock (see CX_CLK_*) */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned owl_clock(void) {
    unsigned *c = imd_ctx(), prd = TICK_PRD, cnt = TICK_CNT, per = prd / 10u;
    if (!per) return c[CX_CLK_MS];
    unsigned prev = c[CX_CLK_PREV];
    c[CX_CLK_ACC] += cnt >= prev ? cnt - prev : cnt + prd - prev;
    c[CX_CLK_PREV] = cnt;
    c[CX_CLK_MS] += c[CX_CLK_ACC] / per;
    c[CX_CLK_ACC] %= per;
    return c[CX_CLK_MS];
}

/* busy wait in milliseconds, calibrated against the system clock while it still ran */
__attribute__((noinline, unused, section(".text.entry")))
static void imd_busy_ms(unsigned ms) {
    unsigned n = ms * imd_ctx()[CX_BLOCKS];
    for (unsigned k = 0; k < n; k++, OWL_CLOCK_HOOK())
        for (volatile unsigned i = 0; i < 1000u; i++) { }
}

__attribute__((noinline, unused, section(".text.entry")))
static void imd_wait_done(void) {
    unsigned n = 0;
    while (!(IMD_SPI & 0x80000000u)) {
        OWL_CLOCK_HOOK();
        if (++n > 3000000u) { imd_ctx()[CX_TIMEOUTS]++; break; }
    }
    IMD_SPI |= 0x40000000u;
}

__attribute__((noinline, unused, section(".text.entry")))
static void imd_cs(unsigned high) {
    unsigned m = imd_ctx()[CX_CS];
    if (high) PA_OUT |= m; else PA_OUT &= ~m;
}

/* one word of 1..4 bytes (right-aligned in `word`), command (dat 0) or data (dat 1) */
__attribute__((noinline, unused, section(".text.entry")))
static void imd_tx(unsigned word, unsigned bytes, unsigned dat) {
    unsigned s = IMD_SPI & ~0x100u & ~0x1000000u;
    if (!(s & 0x800000u)) s = dat ? s | 0x400000u : s & ~0x400000u;
    s = (s & ~0xE000000u) | (((bytes - 1u) & 7u) << 25);
    IMD_SPI = s;
    IMD_BUF = word << ((4u - bytes) * 8u);   /* the unit sends from the top byte down, like the vendor's tx */
    IMD_SPI |= 2u;
    imd_wait_done();
}

/* Command headers as the vendor sends them on the M78 (QSPI, mode 0x21; vendor 0x607abf0 = app 0x7aad0, read on
 * 20.11.2026 after hwtest 1.3.0 drew nothing): register writes 0x02 | command << 8 (0x02002A00, 0x02002B00), the
 * memory write 0x32002C00 on one line, then four lines for the pixels (0x18000, format bits 0x4200).
 * hwtest 1.3.0 took the header from IMD_BUF after the vendor's window: that register reads back shifted by one
 * byte (0x002C0000 for 0x32002C00), so the own driver sent wrong commands and the screen stayed. */
#define IMD_HDR_WRITE(cmd) (0x02000000u | ((cmd) << 8))
#define IMD_HDR_PIXELS 0x32002C00u

__attribute__((noinline, unused, section(".text.entry")))
static void imd_area(unsigned xs, unsigned xe, unsigned ys, unsigned ye) {
    unsigned w = xe - xs + 1u, h = ye - ys + 1u, stride = (2u * w + 3u) & ~3u;
    IMD_CON0 &= ~0x20000000u;                 /* no interrupts: this program waits itself */
    IMD_SPI &= ~0x20000000u;
    IMD_CON1 = (IMD_CON1 & ~0x3FFu) | (w & 0x3FFu);
    IMD_CON1 = (IMD_CON1 & ~(0x3FFu << 10)) | ((h & 0x3FFu) << 10);
    IMD_CON1 = (IMD_CON1 & 0xFFFFFu) | (stride << 20);
    IMD_CON0 = (IMD_CON0 & ~0xFFCu) | ((h & 0x3FFu) << 2);
    IMD_CON3 = (IMD_CON3 & ~0x3FFu) | (h & 0x3FFu);
    unsigned base = (IMD_SPI & ~0x18000u & ~0x7E00u) | 0x4E00u;   /* one line, command format */
    IMD_SPI = base;
    imd_cs(0); imd_tx(IMD_HDR_WRITE(0x2Au), 4u, 0); imd_tx((xs << 16) | xe, 4u, 1); imd_cs(1);
    imd_cs(0); imd_tx(IMD_HDR_WRITE(0x2Bu), 4u, 0); imd_tx((ys << 16) | ye, 4u, 1); imd_cs(1);
    imd_cs(0);
    IMD_SPI = base;
    imd_tx(IMD_HDR_PIXELS, 4u, 0);                                   /* memory write */
    IMD_SPI = (IMD_SPI & ~0x7E00u) | 0x18000u | 0x4200u;             /* four lines from here on */
}

__attribute__((noinline, unused, section(".text.entry")))
static void imd_draw(unsigned char *buf) {
    unsigned *c = imd_ctx();
    IMD_LD = (unsigned)buf;
    IMD_CON0 &= ~0x2000u;
    IMD_SPI = (IMD_SPI & ~0x7E00u) | (c[CX_DRAW_CON] & 0x7E00u);
    IMD_CON0 |= 2u;
    unsigned s = IMD_SPI & ~0x100u;
    if (!(s & 0x800000u)) s |= 0x400000u;
    IMD_SPI = s | 0x1000000u;
    IMD_SPI |= 2u;
    imd_wait_done();
    if (IMD_CON0 & 0x80000000u) IMD_CON0 |= 0x40000000u;
    imd_cs(1);
}

/* a register write with parameters, as lcd_spi_write_cmd does for a QSPI panel: header 0x02 cmd 0x00 on one
 * line, then the bytes, four at a time; chip select around it */
__attribute__((noinline, unused, section(".text.entry")))
static void imd_write_cmd(unsigned cmd, const unsigned char *data, unsigned n) {
    IMD_CON0 &= ~0x20000000u;
    IMD_SPI &= ~0x20000000u;
    IMD_SPI = (IMD_SPI & ~0x18000u & ~0x7E00u) | 0x4E00u;
    imd_cs(0);
    imd_tx(IMD_HDR_WRITE(cmd), 4u, 0);
    while (n) {
        unsigned k = n < 4u ? n : 4u, w = 0;
        for (unsigned i = 0; i < k; i++) w = (w << 8) | data[i];
        imd_tx(w, k, 1);
        data += k; n -= k;
    }
    imd_cs(1);
}

/* the backend text() and owl_rect() draw through (OWL_LCD_SET_AREA / OWL_LCD_DRAW) */
__attribute__((noinline, unused, section(".text.entry")))
static void be_area(unsigned xs, unsigned xe, unsigned ys, unsigned ye) {
    if (imd_ctx()[CX_OWN]) imd_area(xs, xe, ys, ye);
    else ((void (*)(unsigned, unsigned, unsigned, unsigned))0x6022c86u)(xs, xe, ys, ye);
}

__attribute__((noinline, unused, section(".text.entry")))
static void be_draw(unsigned char *buf, unsigned len, unsigned wait) {
    if (imd_ctx()[CX_OWN]) { imd_draw(buf); return; }
    ((void (*)(unsigned char *, unsigned, unsigned))0x6022c76u)(buf, len, wait);
    imd_busy_ms(10u);   /* the vendor draw only starts the transfer */
}

#define OWL_LCD_SET_AREA(xs, xe, ys, ye) be_area((xs), (xe), (ys), (ye))
#define OWL_LCD_DRAW(buf, len, wait) be_draw((buf), (len), (wait))
#define OWL_BUSY_WAIT 1u   /* text()/owl_rect() gaps: be_draw waits itself */
#endif
