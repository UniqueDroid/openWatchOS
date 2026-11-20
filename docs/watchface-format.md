# Watch face files of the M78 (stock firmware)

How the stock firmware's watch face files are built, found by studying the firmware's behaviour and checked against
420 faces of the vendor's market (all readable, 45 different element types used). `fw/owf` implements it.

## File

| Bytes | Content |
| --- | --- |
| 0 | number of elements (the watch reads at most 88) |
| 1 | 0 |
| 2–3 | `JL` |
| from 4 | 24 bytes per element |
| after that | image data the elements point to |

## Element (24 bytes, little endian)

| Offset | Size | Content |
| --- | --- | --- |
| +0 | 1 | type |
| +1 | 1 | 0 = normal, 2 = only in the dimmed always-on mode, 1 = skipped |
| +2, +4 | 2, 2 | x, y |
| +6 | 1 | number of images |
| +7 | 1 | image format: 2 = opaque, 8 = with transparency |
| +8 | 2 | colour or start angle (by type) |
| +10, +12 | 2, 2 | width, height (tap areas: right and bottom edge) |
| +14, +15 | 1, 1 | group / end angle |
| +16 | 4 | offset of the image block |
| +20 | 1 | by type: digit pitch, number of steps, pivot x of a hand, tap action |
| +21 | 1 | alignment: 0 left, 1 centre, 2 right |
| +22 | 1 | by type: y step per digit (signed), pivot y of a hand (16 bit with +23), frame time (16 bit with +23) |
| +23 | 1 | variant (language or day/night image, number of styles) |

## Images

- Image block: (n + 1) 32-bit offsets (the first is 4 (n + 1), the last the block size), then n images.
- Format 2: one 32-bit entry per row (offset in bits 0–20, length in bits 21–31), rows run-length coded: a count
  byte (bit 7 set = one pixel repeated, else that many literal pixels), pixels RGB565 with the high byte first.
- Format 8: pixels of 3 bytes (alpha, then RGB565 high byte first), either with a row table and run-length coding
  like format 2, or raw without a table, rows padded to 4 bytes (hands and other rotated images).
- Formats 5 and 6 occur once each in the market and are not decoded yet.

## Types

| Area | Types |
| --- | --- |
| Images | 0x01 image (variant: day/night or language), 0x02 animation, 0x03 style image |
| Digital time | 0x32 hour, 0x33 minute, 0x3b second, 0x3e/0x3f hour tens/ones, 0x40/0x41 minute tens/ones, 0x3a hour as image number, 0x67 AM/PM |
| Hands | 0x36 hour, 0x37 minute, 0x38 second (jumping), 0x39/0x3d second (gliding), 0x42 24-hour, 0x44 day |
| Date | 0x34 month, 0x35 day, 0x3c year, 0x63/0x64 weekday |
| Device | 0x65 Bluetooth, 0x66 battery steps, 0x68 battery percent, 0x9c unread messages |
| Weather | 0x6a symbol, 0x6b/0x6d temperature, 0x6c unit |
| Activity | 0x96 steps, 0x97 kcal, 0x99 distance, 0xa0/0xa1 progress steps/kcal |
| Health | 0x98 pulse, 0xa3 pulse scale, 0x9e blood oxygen, 0xa4 its scale, 0x9a/0x9b sleep hours/minutes |
| Compass | 0x94 degrees, 0x95 direction |
| Touch | 0xb4 tap area (+20 = action, e.g. 1 steps, 2 pulse, 3 sleep, 6 weather, 8 next style, 12 settings) |

Numbers: total width = pitch × (digits − 1) + width; alignment 1 centres on x, 2 ends at x. Hands: x, y is the
pivot on the screen, +20/+22 the pivot in the image; they turn clockwise. The first tap area containing the point
wins.
