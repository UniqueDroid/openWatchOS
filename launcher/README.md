# Launcher programs (OWLP)

Small native programs that a JS app on the watch starts with `launch(hex)`, or that the watch starts right after
power-on while the left key is held (start program). They need the modified M78 firmware 4.50.86 or later; that
firmware is not part of this repository.

## Format

    "OWLP" | version | code length | CRC-32 of the code | entry offset      (5 × 32 bit, little endian)
    code

- Version 1: the program never returns (the watch restarts afterwards).
- Version 2: the program returns to the app.
- At most 16 KB of code.
- The watch copies the code to an unknown address in PSRAM and jumps to the entry offset. So the code must be
  position independent:
  - only the section `.text.entry`;
  - no relocations;
  - no global variables;
  - no `.rodata`.
- Constant data and state live inside the code section (`OWL_BLOB` in `owl_font.h`): a call puts the address of
  the data in the return register, so `name() + 2` is the data at run time.

`tools/build_owlp.py` checks all of this and refuses anything else.

## Files

| File | What it is |
| --- | --- |
| `owl_sys.h` | time, watchdog, keys, waiting |
| `owl_lcd.h` | drawing through the stock display driver, rectangles |
| `owl_font.h` | 5×7 font (ASCII 32–90) and `OWL_BLOB` |
| `owl_text.h` | text, numbers, scaling |
| `owl_imd.h` | **own display driver** (display unit registers, chip select, own clock) |
| `owl_solo.h` | stop and resume the stock firmware |
| `owl_i2c.h` | **own software I²C** and touch reading |
| `hw_common.h` | colours, include order |
| `hw_ui.c` | **UI 1**: watch face, menu, stopwatch, lamp, paint, info; swipe and tap |
| `hw_solo.c` | test: own display driver with the stock firmware stopped |
| `hw_touch_solo.c` | test: own touch reading |
| `hw_lcd_init.c` | test: switch the panel off, reset it, run the own power-on sequence |

## Build

    JL_TOOLCHAIN=/path/to/jieli/common/bin python3 tools/build_owlp.py launcher/hw_ui.c --version 2

This writes `launcher/hw_ui.owl` and prints it as hex text. That hex text is what `launch()` and `bootSave()`
take.

## Status (20.11.2026)

All files above were tested on a real M78:

- display, touch and keys work with our own code;
- the clock from the tick timer kept the time;
- the stock firmware takes over again afterwards without a restart.
