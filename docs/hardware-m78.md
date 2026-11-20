# M78 hardware (Hello Watch 3+ and relatives)

What we know about the board, found by reading the stock firmware's behaviour and by tests on the watch.
"Confirmed" means: seen on a real watch with our own code. Everything else is read from the firmware only.

The ND326 firmware uses the same key and display pins, so it is very likely the same board.

## Chip

JieLi JL701N (AC701N, family BR28), pi32v2 cores (two), internal flash 2 MB, PSRAM, external NOR flash on port C
(QSPI) for resources.

## Display (confirmed)

- AMOLED, **368 × 448**, RGB565, connected by **QSPI** (four data lines). Driver name in the firmware: **GP3112**;
  the command set matches the SH8601 family.
- Pins: **reset PA02**, **chip select PA06**, power **PA05**, no TE line connected, no D/C line.
- Pixel order in memory: RGB565 **high byte first**.
- Windows must start at an even column and row and have an even width and height. With odd sizes the
  picture comes out slanted (the display unit rounds the bytes per row up to a multiple of 4).
- Brightness: command `51 <0..FF>`; there is no backlight.

Power-on sequence (confirmed: our own code switches the panel off, resets it and brings it back with this):

    11                 sleep out
    wait 100 ms
    C0 5A 5A           unlock vendor commands
    B0 08
    B1 F0 00
    C0 A5 A5           lock vendor commands
    2A 00 00 01 6F     columns 0..367
    2B 00 00 01 BF     rows 0..447
    44 00 0A           tear line 10
    35 00              tear signal on
    4A FF
    3A 55              16 bits per pixel
    53 20              brightness control on
    wait 25 ms
    29                 display on
    51 FF              full brightness

Panel reset: PA02 low for 20 ms, then high, wait 150 ms.

### Display unit of the chip (IMD, base `0xfe2d00`)

How the chip sends commands and pixels to a QSPI panel. Our own driver is in `launcher/owl_imd.h`:

| Offset | Register | Use |
| --- | --- | --- |
| `+0x00` | CON0 | start, fill with one colour, interrupt, window height |
| `+0x04` | CON1 | window width (bits 0–9), height (10–19), bytes per row (20–31) |
| `+0x0c` | CON3 | window height |
| `+0x10` | LD | address of the pixels |
| `+0x20` | SPI | SPI control: bit 31 done, bit 30 clear done, bits 25–27 byte count − 1, bit 24 pixels from LD, bits 15/16 four lines, bits 9–13 format |
| `+0x24` | BUF | word to send, left-justified |

How a window is drawn:

1. Send `2A` (columns) and `2B` (rows) as a write command on one line.
2. Send `2C` with the pixels from LD.
3. Send the pixels on four lines.

Chip select (PA06) is a plain GPIO that the code pulls low around each transfer.

## Touch (confirmed)

- Chipsea CHSC family. Software I²C: **SCL PG03, SDA PG04**. Interrupt **PG05**, reset **PG06**.
- I²C address `0x2E`. To read, send address byte `0x5D`, then read 3 bytes, without selecting a register first.
  - Byte 0: event. Low 2 bits = 1 means a finger. Bit 6 is the ninth bit of x, bit 7 the ninth bit of y.
  - Byte 1: x (low 8 bits).
  - Byte 2: y (low 8 bits).
- The controller only answers while a finger is on the screen. When it stops answering, the finger is gone.
- Writes use address byte `0x5C`, then a 16-bit register (high byte first), then the data.

## Keys (confirmed)

All three keys are pulled up: **low = pressed**.

| Key | Pin | Port IN register |
| --- | --- | --- |
| left side key | **PB01** | `0xfd5104` bit 1 |
| right side key | **PG08** | `0xfd5504` bit 8 |
| crown (press) | **PB04** | `0xfd5104` bit 4 |

- Holding the crown for **8 seconds** restarts the chip in hardware (chip reset pin). This works even when our own
  code hangs.
- Crown rotation: two GPIOs, **PC06** and **PB07**. The stock firmware decodes them itself and does not use the
  chip's rotary decoder (RDEC).

## Other parts

| Part | What we know |
| --- | --- |
| Vibration motor | PWM on **PB06** (confirmed: our code can stop it) |
| Accelerometer | SC7A20 (driver in the firmware) |
| Heart rate / wear detection | Vcare LC09A, I²C address `0x33`, green light only |
| Microphone | present |
| Battery | chip's built-in charger and VBAT measurement, no extra pins |

## GPIO registers

Port A starts at `0xfd5000`, port B at `0xfd5100`, port G at `0xfd5500`. In each port: OUT `+0`, IN `+4`, DIR `+8`
(DIR bit 1 = input).

## Still open

- Exact panel and touch chip IDs (read the ID registers on the watch).
- Interrupt pins of the sensors.
- Battery voltage limits and charge parameters.
