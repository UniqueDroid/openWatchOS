# What OWLP programs use from the stock firmware

OWLP programs (see `launcher/README.md`) run inside the stock firmware. They call some of its functions by their
fixed address. This file lists those addresses and what each one does. It gives addresses and behaviour only; no
code from the firmware is in this repository.

The addresses fit the firmware line 4.50.x for the M78, which the launcher needs. They change with every other
firmware build.

## Functions

| Address | Function | What it does |
| --- | --- | --- |
| `0x11f36e` | `jiffies_msec()` | milliseconds since start (code in RAM) |
| `0x6009b4c` | `wdt_clear()` | feeds the watchdog; without it the watch restarts after about 10 s of busy code |
| `0x1fdac` | `flush_dcache(buf, len)` | writes the data cache back before the display unit reads a buffer |
| `0x6003790` | `motor_duty(duty)` | vibration motor; 0 = off |
| `0x6003d12` | `rtc_now_seconds()` | seconds since 2000-01-01 from the RTC |
| `0x11f76e` | `cpu_suspend_other_core(85)` | parks the second core; afterwards this core runs with interrupts off |
| `0x11f84c` | `cpu_resume_other_core(85)` | lets the stock firmware run again |

## Display driver of the stock firmware

| Address | Function | What it does |
| --- | --- | --- |
| `0x6022c86` | `set_draw_area(xs, xe, ys, ye)` | sets the window, inclusive coordinates |
| `0x6022c76` | `draw(buf, len, wait)` | starts the transfer of a window; it does **not** wait, so wait about 10 ms before the next window |
| `0x6022cda` | `clear_screen(0xRRGGBB)` | fills the whole screen in hardware |
| `[0x106430]` | driver config | `+40` pointer to the driver's draw buffer, `+36` its size in bytes |

Drawing into the driver's own buffer avoids allocating memory. An earlier test took a new PSRAM block on every run
and never freed it, so after a few runs drawing stopped working until the next restart.

## Data the stock firmware keeps

| Address | Content |
| --- | --- |
| `0x108938` / `0x10893c` | last touch x / y (written by the stock touch interrupt) |
| `0x108918` | touch state |

## Running without the stock firmware

The UI (`launcher/hw_ui.c`) works without the stock firmware for as long as it runs:

1. Read the RTC once.
2. Switch the motor off.
3. Park the second core with `cpu_suspend_other_core(85)`.
4. From then on, use only our own code:
   - display: `owl_imd.h`
   - touch: `owl_i2c.h`
   - keys: port registers
   - clock: the core tick timer (`0xffe0f0` count, `0xffe0f4` period of 10 ms)
5. Call `cpu_resume_other_core(85)` to hand the watch back to the stock firmware.

All of this is confirmed on the watch. During the tests nothing restarted.
