/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_SYS_H
#define OWL_SYS_H
/* openWatchOS launcher: time, watchdog and keys for OWLP programs (vendor firmware 4.50.x).
 *
 * Time: jiffies_msec 0x11f36e (RAM code; found through gettimeofday 0x60e8bd8, matched against the SDK's
 * function listing; it calls it twice). Milliseconds since start. hwtest 1.0.0 had
 * 0x11f76e (a slip when adding up the call offset): every test restarted the watch at once. The RAM code sits at
 * app offset + 0x136d8 in the image (code at app 0x132a46: "[--sp] = rets; call; r0 >>= 1; pc = [sp++]").
 * Watchdog: wdt_clear 0x6009b4c (same matching: "[0xf2a800] |= 0x10000"). A program that keeps the GUI task busy
 * for long must clear it: the watch restarted after about 10 s in the touch test of rueckweg 1.7.0 (Jan,
 * 19.11.2026), most likely the watchdog, because the task that normally clears it did not get to run.
 * Keys: port IN registers, low = pressed (docs/hardware-m78.md): crown PB04, left PB01, right PG08.
 */
typedef unsigned (*owl_ms_fn)(void);
typedef void (*owl_void_fn)(void);

#define OWL_MS ((owl_ms_fn)0x11f36eu)
#define OWL_WDT_CLEAR ((owl_void_fn)0x6009b4cu)
#define OWL_PORT_B (*(volatile unsigned *)0xfd5104u)
#define OWL_PORT_G (*(volatile unsigned *)0xfd5504u)
#define OWL_KEY_CROWN 1u
#define OWL_KEY_LEFT 2u
#define OWL_KEY_RIGHT 4u
#define OWL_EXIT_HOLD_MS 2000u      /* crown held this long = back to the app */
#define OWL_MAX_RUN_MS 300000u      /* every program returns after 5 minutes at the latest */

#ifdef OWL_BUSY_WAIT
/* diagnosis: wait by counting, no clock, no watchdog. One fixed wait (OWL_BUSY_WAIT loops, a 32-bit constant the
 * model can find and shorten), long enough for one window as in rueckweg 1.6.0. */
__attribute__((noinline, unused, section(".text.entry")))
static void owl_wait_ms(unsigned ms) {
    (void)ms;
    for (volatile unsigned i = 0; i < OWL_BUSY_WAIT; i++) { }
}
#else
/* waits at least `ms` milliseconds, clearing the watchdog meanwhile */
__attribute__((noinline, unused, section(".text.entry")))
static void owl_wait_ms(unsigned ms) {
    unsigned start = OWL_MS();
    do { OWL_WDT_CLEAR(); } while (OWL_MS() - start <= ms);
}
#endif

/* keys held right now: OWL_KEY_* bits */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned owl_keys(void) {
    unsigned b = OWL_PORT_B, g = OWL_PORT_G;
    return ((b >> 4) & 1u ? 0u : OWL_KEY_CROWN) | ((b >> 1) & 1u ? 0u : OWL_KEY_LEFT) | ((g >> 8) & 1u ? 0u : OWL_KEY_RIGHT);
}

/* call once per loop with the keys held; 1 = leave (crown held 2 s, or 5 minutes over). *crown_since starts at 0. */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned owl_should_exit(unsigned keys, unsigned started, unsigned *crown_since) {
    unsigned now = OWL_MS();
    OWL_WDT_CLEAR();
    if (now - started >= OWL_MAX_RUN_MS) return 1u;
    if (!(keys & OWL_KEY_CROWN)) { *crown_since = 0; return 0u; }
    if (!*crown_since) { *crown_since = now | 1u; return 0u; }
    return now - *crown_since >= OWL_EXIT_HOLD_MS;
}
#endif
