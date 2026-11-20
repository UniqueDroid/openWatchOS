/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef HW3_JS_H
#define HW3_JS_H
/*
 * JavaScript on the watch: MicroQuickJS (development/js/mquickjs, MIT licence, Fabrice Bellard and
 * Charlie Gordon; commit 4119f68 of https://github.com/bellard/mquickjs). The engine keeps all
 * of its state in the memory block it is given; it has no writable data of its own and runs in
 * place from flash. Confirmed on the watch with firmware 4.50.45 (self-test result 42).
 *
 * A script is a program that defines global functions the watch calls:
 *   draw()      on every redraw of its screen
 *   tap(x, y)   on a tap
 *   tick()      every setTimer period, followed by a redraw
 * and may call:
 *   fillRect(x1, y1, x2, y2, 0xRRGGBB)   screen is 368 x 448
 *   drawText(x, y, string)               white, centred on x
 *   time()                               [year, month, day, hour, minute, second, weekday 1 = Monday]; permission time
 *   setTimer(ms)                         tick() every ms (100..60000), 0 stops; permission timer
 *   vibrate(ms)                          10..2000 ms; permission vibrate
 *   save(text)                           keeps up to 1024 bytes of text for this app, true when stored; permission storage
 *   load()                               the text saved last, '' when there is none; permission storage
 *   send(text)                           up to 200 bytes of text to the app's companion script on the phone, true when sent;
 *                                        permission phone.message. The watch calls message(text) when the companion answers.
 *   noise()                              loudness at the microphone in tenths of dB (uncalibrated), -1 while there is no value
 *                                        yet, -2 when the microphone is not available or 10 minutes are over; noise(1) starts
 *                                        again. The microphone runs while the app keeps asking (it stops 3 s after the last
 *                                        call), the display stays on and draw() is called twice a second; permission microphone
 *   peek(address)                        32-bit word of the watch's RAM at a multiple of 4 in 0x100000..0x11fffc, as a signed
 *                                        number, or the IN register of a GPIO port (0xfd5004 + 0x100 * port: A B C D E G P,
 *                                        not 0xfd5604); read only, for diagnostics (pin tables, key test); permission debug
 *   keys()                               the key values of the last (up to 16) key presses, oldest first, as "0 2 1";
 *                                        logged by the watch's key scan even while other screens are shown; permission debug
 *   launch(hex)                          starts an OWLP program (tools/build_launch_payload.py, as hex text) from the
 *                                        PSRAM; does not return when it starts, else a negative error; permission boot
 *   bootSave(hex)                        stores an OWLP program as start program (runs after a restart while the left key
 *                                        is held); "" removes it; 0 = stored, -1/-2 = no valid program, -7 = file error;
 *                                        permission boot
 *   bootSize()                           code bytes of the stored start program, 0 = none; permission boot
 * A call without its permission throws a TypeError.
 */
#include <stdint.h>

/* Runs a fixed script in `memory`; 42 when the engine works, negative on failure. */
int hw3_js_selftest(void *memory, unsigned size);
/* Creates the engine in `memory` and runs the script once. `source` must be 0-terminated after `length` bytes.
 * 0, or negative on failure (-1 no context, -2 script error). */
int hw3_js_open(void *memory, unsigned size, const char *source, unsigned length);
/* Calls the script's global function `name` with up to two integer arguments.
 * 1 called, 0 no such function, negative: it failed (-1 stack, -2 exception). */
int hw3_js_call(void *memory, const char *name, int argc, int a0, int a1);
/* Calls the script's global function `name` with one string argument (UTF-8, `length` bytes). Same results. */
int hw3_js_call_text(void *memory, const char *name, const char *text, unsigned length);

/* Provided by the platform (watch: hw3_js_watch.c; tests: their own). Text is UTF-16, 0-terminated. */
#define HW3_JS_TEXT 48u
void hw3_js_host_rect(int x1, int y1, int x2, int y2, uint32_t rgb);
void hw3_js_host_text(int x, int y, const uint16_t *text);
/* Permissions: bit numbers in the order of the list in tools/pack_js_app.py and jsapp.c. */
#define HW3_JS_PERM_TIME 0u
#define HW3_JS_PERM_TIMER 1u
#define HW3_JS_PERM_VIBRATE 2u
#define HW3_JS_PERM_STORAGE 4u
#define HW3_JS_PERM_PHONE 10u
#define HW3_JS_PERM_MICROPHONE 11u
#define HW3_JS_PERM_DEBUG 12u
#define HW3_JS_PERM_BOOT 13u
#define HW3_JS_MESSAGE 200u                           /* bytes of text of one message */
#define HW3_JS_STORE 1024u                            /* bytes of text an app can keep */
int hw3_js_host_allowed(unsigned permission);          /* 1 when the running script holds it */
uint32_t hw3_js_host_seconds(void);                    /* local time, seconds since 2000-01-01 */
void hw3_js_host_timer(unsigned milliseconds);         /* 0 stops */
void hw3_js_host_vibrate(unsigned milliseconds);
int hw3_js_host_save(const char *text, unsigned length);        /* 1 when stored */
unsigned hw3_js_host_load(char *out, unsigned capacity);        /* bytes read, 0 when there is nothing */
int hw3_js_host_send(const char *text, unsigned length);        /* 1 when handed to the phone link */
#define HW3_JS_KEYS 16u
unsigned hw3_js_host_keys(char *out, unsigned capacity);
/* OWLP program as hex text: checks it, copies it into the PSRAM and jumps. Version 1 runs with interrupts off and
 * returns only on errors: -1 format, -2 checksum, -3 no PSRAM, -4 PSRAM block outside 0x2000000..0x2200000,
 * -5 the program returned. Version 2 keeps interrupts on and gives 0 when the program comes back. */
int hw3_js_host_launch(const char *hex, unsigned length);
int hw3_js_host_boot_save(const char *hex, unsigned length);   /* start program, see bootSave */
int hw3_js_host_boot_size(void);       /* "v v v": last key values pressed, oldest first */
int hw3_js_host_peek(uint32_t address, uint32_t *value);       /* 1 and the word when the address may be read */
int hw3_js_host_noise(int restart);                             /* tenths of dB, -1 none yet, -2 not available */

/* Local seconds since 2000-01-01 -> year, month, day, hour, minute, second, weekday (1 = Monday). */
void hw3_js_calendar(uint32_t seconds, int out[7]);
#endif
