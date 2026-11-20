/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_SOLO_H
#define OWL_SOLO_H
/* openWatchOS launcher: running without the vendor firmware for a while (hwtest "Eigenstaendig", 20.11.2026).
 * Include after hw_common.h (needs owl_imd.h, owl_sys.h, owl_lcd.h).
 *   imd_learn()           one window by the vendor driver, watched: pixel format bits, chip select, loops per ms
 *   OWL_SUSPEND_VENDOR()  motor off, then cpu_suspend_other_core(85), vendor 0x11f76e: second core parked, interrupts
 *                         of this core off
 *   OWL_RESUME_VENDOR()   cpu_resume_other_core(85), vendor 0x11f84c
 * Checked on the watch with hwtest 1.3.1: own display driver and keys for 64 s, back without a restart.
 */
/* Motor off first (vendor motor_duty 0x6003790 with duty 0, as in the vibration test): the start program runs right
 * after the start, while the vendor's start vibration is still on; stopped in the middle, it went on vibrating
 * until the vendor came back (Jan, 20.11.2026, firmware 4.50.87). */
#define OWL_SUSPEND_VENDOR() (((void (*)(unsigned))0x6003790u)(0u), ((void (*)(unsigned))0x11f76eu)(85u))
#define OWL_RESUME_VENDOR() ((void (*)(unsigned))0x11f84cu)(85u)

/* step 1: one window by the vendor driver, watched */
__attribute__((noinline, unused, section(".text.entry")))
static void imd_learn(void) {
    unsigned *c = imd_ctx();
    unsigned start = OWL_MS(), blocks = 0;
    while (OWL_MS() - start < 200u) {
        for (volatile unsigned i = 0; i < 1000u; i++) { }
        blocks++;
    }
    c[CX_BLOCKS] = blocks / 200u;
    unsigned short *buf = OWL_LCD_BUF;
    for (unsigned k = 0; k < OWL_W * 2u; k++) buf[k] = 0;
    OWL_FLUSH_DCACHE(buf, OWL_W * 4u);
    OWL_VENDOR_SET_AREA(0, OWL_W - 1u, OWL_H - 2u, OWL_H - 1u);   /* two black rows at the bottom edge */
    c[CX_AREA_CON] = IMD_SPI; c[CX_AREA_BUF] = IMD_BUF; c[CX_PA_LOW] = PA_OUT; c[CX_CON1] = IMD_CON1;
    OWL_VENDOR_DRAW((unsigned char *)buf, OWL_W * 4u, 1);
    c[CX_DRAW_CON] = IMD_SPI;
    imd_busy_ms(50u);
    c[CX_PA_HIGH] = PA_OUT;
    unsigned cs = c[CX_PA_HIGH] & ~c[CX_PA_LOW] & 0xFFFFu;
    c[CX_CS] = cs && !(cs & (cs - 1u)) ? cs : 1u << 6;   /* exactly one bit, else PA06 from the board table */
}

#endif
