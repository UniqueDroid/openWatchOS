/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OWL_I2C_H
#define OWL_I2C_H
/* openWatchOS launcher: own software I2C for the touch controller (only while the vendor firmware is stopped:
 * the vendor reads the same lines in its touch interrupt).
 *
 * Pins as the vendor's soft I2C uses them (stop routine app 0x10c154, read 0x1826): SCL = PG03 (gpio 83),
 * SDA = PG04 (gpio 84), port G registers 0xfd5500 (OUT +0, IN +4, DIR +8, DIR bit 1 = input).
 * Open drain by direction: a line is driven low with OUT 0 / DIR 0, and let go with DIR 1 (pull-up from the
 * vendor's set-up). Touch controller: address 0x2E, read = 0x5D, then 3 bytes (docs/hardware-m78.md):
 * byte 0 event (low 2 bits 1 = finger, bit 6/7 = ninth bit of x/y), byte 1 x, byte 2 y.
 * Timing: 5 x CX_BLOCKS busy loops per line change (CX_BLOCKS = thousands of loops per ms, from imd_learn; 21 on
 * the M78, so about 5 us, roughly 50-100 kHz). Needs owl_imd.h.
 */
#define PG_OUT (*(volatile unsigned *)0xfd5500u)
#define PG_IN (*(volatile unsigned *)0xfd5504u)
#define PG_DIR (*(volatile unsigned *)0xfd5508u)
#define I2C_SCL (1u << 3)
#define I2C_SDA (1u << 4)

__attribute__((noinline, unused, section(".text.entry")))
static void i2c_delay(void) {
    unsigned n = imd_ctx()[CX_BLOCKS] * 5u;
    for (volatile unsigned i = 0; i < n; i++) { }
}

__attribute__((noinline, unused, section(".text.entry")))
static void i2c_line(unsigned bit, unsigned high) {
    if (high) PG_DIR |= bit;
    else { PG_OUT &= ~bit; PG_DIR &= ~bit; }
    i2c_delay();
}

__attribute__((noinline, unused, section(".text.entry")))
static void i2c_start(void) {
    i2c_line(I2C_SDA, 1); i2c_line(I2C_SCL, 1);
    i2c_line(I2C_SDA, 0); i2c_line(I2C_SCL, 0);
}

__attribute__((noinline, unused, section(".text.entry")))
static void i2c_stop(void) {
    i2c_line(I2C_SDA, 0); i2c_line(I2C_SCL, 1); i2c_line(I2C_SDA, 1);
}

/* nine clocks with SDA let go, then a stop: frees a slave that was cut off in the middle of a byte */
__attribute__((noinline, unused, section(".text.entry")))
static void i2c_recover(void) {
    i2c_line(I2C_SDA, 1);
    for (unsigned i = 0; i < 9u; i++) { i2c_line(I2C_SCL, 1); i2c_line(I2C_SCL, 0); }
    i2c_stop();
}

/* one byte out, returns 1 if the slave acknowledged */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned i2c_send(unsigned byte) {
    for (unsigned i = 0; i < 8u; i++) {
        i2c_line(I2C_SDA, (byte >> (7u - i)) & 1u);
        i2c_line(I2C_SCL, 1); i2c_line(I2C_SCL, 0);
    }
    i2c_line(I2C_SDA, 1);
    i2c_line(I2C_SCL, 1);
    unsigned ack = !(PG_IN & I2C_SDA);
    i2c_line(I2C_SCL, 0);
    return ack;
}

__attribute__((noinline, unused, section(".text.entry")))
static unsigned i2c_receive(unsigned ack) {
    unsigned byte = 0;
    i2c_line(I2C_SDA, 1);
    for (unsigned i = 0; i < 8u; i++) {
        i2c_line(I2C_SCL, 1);
        byte = (byte << 1) | ((PG_IN & I2C_SDA) ? 1u : 0u);
        i2c_line(I2C_SCL, 0);
    }
    i2c_line(I2C_SDA, ack ? 0u : 1u);
    i2c_line(I2C_SCL, 1); i2c_line(I2C_SCL, 0);
    i2c_line(I2C_SDA, 1);
    return byte;
}

/* touch: 1 = read, out[0..2] filled; 0 = no answer */
__attribute__((noinline, unused, section(".text.entry")))
static unsigned touch_read(unsigned char *out) {
    i2c_start();
    if (!i2c_send(0x5Du)) { i2c_stop(); return 0; }
    for (unsigned i = 0; i < 100u; i++) i2c_delay();   /* the vendor waits a little here as well */
    out[0] = (unsigned char)i2c_receive(1);
    out[1] = (unsigned char)i2c_receive(1);
    out[2] = (unsigned char)i2c_receive(0);
    i2c_stop();
    return 1;
}
#endif
