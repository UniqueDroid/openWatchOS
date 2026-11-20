# openWatchOS

A free replacement firmware for the **M78** smartwatch (sold as Hello Watch 3+; JieLi JL701N chip, 368 × 448 AMOLED),
built step by step.

So far:

- **Own display, touch and key code** runs on the watch while the stock firmware is paused.
- **UI 1:** a first own surface with watch face, menu, stopwatch, lamp, paint and info, operated by swipe and tap.
  It can run as a start program right after power-on.
- **Hardware description:** pins, panel power-on sequence, touch protocol, display unit registers.

The programs still start from inside the stock firmware (OWLP launcher). The goal is a complete firmware of our own.

## Contents

| Path | Content |
| --- | --- |
| `docs/hardware-m78.md` | board and parts: display, touch, keys, motor, pins |
| `docs/firmware-interface.md` | the few stock firmware functions OWLP programs call, and how the stock firmware is paused |
| `launcher/` | OWLP programs and their building blocks (own drivers, font, text, UI 1) |
| `tools/build_owlp.py` | builds an OWLP program with the JieLi LLVM toolchain |
| `oapps/` | JS apps for the watch |

## What is not here

Firmware images, code or parts of the vendors' SDKs and libraries, and decompiled code are not part of this
repository. Everything here is written by us. What we found out about the hardware is described in our own words.

## License

GNU General Public License, version 2. See [LICENSE](LICENSE).
