/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS watch face engine, see owf.h and docs/watchface-format.md.
 *
 * Element (24 bytes, little endian): +0 type, +1 flags (2 = only in the dimmed always-on mode, 1 = skipped),
 * +2 x, +4 y, +6 image count, +7 image format, +8 colour/angle, +10 width, +12 height, +14/+15 group,
 * +16 offset of the image block, +20 "count" (digit pitch, steps, pivot x, tap action), +21 alignment,
 * +22 "p16" (y step per digit, pivot y low byte, frame time low byte), +23 "p17" (variant, pivot y high byte).
 * Image block: (n + 1) 32-bit offsets (first = 4 (n + 1), last = block size), then n images.
 * Format 2: opaque RGB565, a 32-bit entry per row (offset bits 0..20, length bits 21..31), rows run-length coded
 * (count byte: bit 7 = one pixel repeated, else literal pixels), pixels high byte first.
 * Format 8: like format 2 with 3-byte pixels (alpha, RGB565 high byte first), or raw without a row table, rows
 * padded to 4 bytes (hands).
 * The behaviour per type follows the stock interpreter as described in the format document; it was cross-checked
 * against our Python reference renderer.
 */
#include "owf.h"

#define RD16(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8)
#define RD32(p) (RD16(p) | RD16((p) + 2) << 16)

typedef struct el {
    uint8_t type, flags, images, format, count, align, p16, p17;
    int16_t x, y;
    uint16_t w, h;
    uint32_t offset;
} el;

typedef struct ctx {
    const uint8_t *f;
    uint32_t len;
    const owf_band *b;
    int bad;
} ctx;

static void load(const uint8_t *r, el *e) {
    e->type = r[0]; e->flags = r[1];
    e->x = (int16_t)RD16(r + 2); e->y = (int16_t)RD16(r + 4);
    e->images = r[6]; e->format = r[7];
    e->w = (uint16_t)RD16(r + 10); e->h = (uint16_t)RD16(r + 12);
    e->offset = RD32(r + 16);
    e->count = r[20]; e->align = r[21]; e->p16 = r[22]; e->p17 = r[23];
}

int owf_check(const uint8_t *f, uint32_t len) {
    if (!f || len < 4 || f[2] != 'J' || f[3] != 'L') return -1;
    uint32_t n = f[0];
    if (n > OWF_MAX_ELEMENTS) n = OWF_MAX_ELEMENTS;
    if (4u + 24u * n > len) return -2;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t off = RD32(f + 4 + 24 * i + 16);
        if (off && (off < 4u + 24u * n || off >= len)) return -3;
    }
    return 0;
}

/* ---- pixels ---- */

static uint32_t c5(uint32_t v, uint32_t bits) { return bits == 5 ? (v * 255u + 15u) / 31u : (v * 255u + 31u) / 63u; }

static void plot(const ctx *c, int x, int y, uint32_t rgb, uint32_t a) {
    if (x < 0 || x >= OWF_W || y < c->b->y0 || y >= c->b->y0 + c->b->rows || !a) return;
    uint16_t *p = c->b->px + (y - c->b->y0) * OWF_W + x;
    if (a < 255u) {
        uint32_t d = c->b->swap ? (uint32_t)((*p >> 8) | (*p << 8)) & 0xFFFFu : *p;
        /* blend in 8 bits per channel, as an RGBA canvas does, then back to RGB565 */
        uint32_t sr = c5(rgb >> 11, 5), sg = c5((rgb >> 5) & 63u, 6), sb = c5(rgb & 31u, 5);
        uint32_t dr = c5(d >> 11, 5), dg = c5((d >> 5) & 63u, 6), db = c5(d & 31u, 5);
        uint32_t r = (sr * a + dr * (255u - a) + 127u) / 255u, g = (sg * a + dg * (255u - a) + 127u) / 255u,
                 bl = (sb * a + db * (255u - a) + 127u) / 255u;
        rgb = (r >> 3) << 11 | (g >> 2) << 5 | bl >> 3;
    }
    *p = c->b->swap ? (uint16_t)((rgb >> 8) | (rgb << 8)) : (uint16_t)rgb;
}

/* image `index` of the element's block: start and length, 0 = missing */
static const uint8_t *image(const ctx *c, const el *e, uint32_t index, uint32_t *size) {
    if (!e->offset || e->offset + 8u > c->len) return 0;
    const uint8_t *blk = c->f + e->offset;
    uint32_t first = RD32(blk);
    if (first < 8u || (first & 3u) || e->offset + first > c->len || index + 1u >= first / 4u) return 0;
    uint32_t a = RD32(blk + 4 * index), b = RD32(blk + 4 * index + 4), last = RD32(blk + first - 4);
    if (b < a || e->offset + last > c->len || b > last) return 0;
    *size = b - a;
    return blk + a;
}

/* one row of a run-length coded image (px = 2 or 3 bytes per pixel): calls out() per pixel run.
 * Returns the number of pixels, or -1 if the row is broken. */
static int rle_row(const uint8_t *img, uint32_t size, uint32_t h, uint32_t row, uint32_t px,
                   const ctx *c, int x0, int y, int draw) {
    if (size < 4u * h) return -1;
    uint32_t entry = RD32(img + 4 * row), at = entry & 0x1FFFFFu, end = at + (entry >> 21);
    if (at < 4u * h || end > size) return -1;
    int n = 0;
    while (at < end) {
        uint32_t run = img[at++], k = run & 127u;
        if (!k || at + px * ((run & 128u) ? 1u : k) > end) return -1;
        for (uint32_t i = 0; i < ((run & 128u) ? 1u : k); i++) {
            uint32_t a = px == 3u ? img[at] : 255u, v = (uint32_t)img[at + px - 2] << 8 | img[at + px - 1];
            at += px;
            uint32_t times = (run & 128u) ? k : 1u;
            if (draw) for (uint32_t t = 0; t < times; t++) plot(c, x0 + n + (int)t, y, v, a);
            n += (int)times;
        }
    }
    return n;
}

/* raw format 8 without a row table? (the rows of a run-length image are checked like the reference does) */
static int is_raw(const uint8_t *img, uint32_t size, const el *e) {
    uint32_t stride = (3u * e->w + 3u) & ~3u;
    if (size < stride * e->h) return 0;
    for (uint32_t r = 0; r < e->h; r++)
        if (rle_row(img, size, e->h, r, 3, 0, 0, 0, 0) != (int)e->w) return 1;
    return 0;
}

static void put(ctx *c, const el *e, uint32_t index, int x, int y) {
    uint32_t size;
    const uint8_t *img = image(c, e, index, &size);
    if (!img) return;
    if (e->format != 2 && e->format != 8) { c->bad = 1; return; }
    int top = y > c->b->y0 ? y : c->b->y0, bottom = y + e->h < c->b->y0 + c->b->rows ? y + e->h : c->b->y0 + c->b->rows;
    if (top >= bottom) return;
    if (e->format == 8 && is_raw(img, size, e)) {
        uint32_t stride = (3u * e->w + 3u) & ~3u;
        for (int yy = top; yy < bottom; yy++) {
            const uint8_t *p = img + (uint32_t)(yy - y) * stride;
            for (uint32_t i = 0; i < e->w; i++, p += 3) plot(c, x + (int)i, yy, (uint32_t)p[1] << 8 | p[2], p[0]);
        }
        return;
    }
    for (int yy = top; yy < bottom; yy++)
        if (rle_row(img, size, e->h, (uint32_t)(yy - y), e->format == 8 ? 3u : 2u, c, x, yy, 1) != (int)e->w) { c->bad = 1; return; }
}

/* ---- hands: rotation about the pivot, clockwise, bilinear ---- */

static const int16_t SIN[91] = {   /* sin(d) * 16384, d = 0..90 */
    0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563, 2845, 3126, 3406, 3686, 3964, 4240, 4516, 4790, 5063, 5334,
    5604, 5872, 6138, 6402, 6664, 6924, 7182, 7438, 7692, 7943, 8192, 8438, 8682, 8923, 9162, 9397, 9630, 9860, 10087,
    10311, 10531, 10749, 10963, 11174, 11381, 11585, 11786, 11982, 12176, 12365, 12551, 12733, 12911, 13085, 13255,
    13421, 13583, 13741, 13894, 14044, 14189, 14330, 14466, 14598, 14726, 14849, 14968, 15082, 15191, 15296, 15396,
    15491, 15582, 15668, 15749, 15826, 15897, 15964, 16026, 16083, 16135, 16182, 16225, 16262, 16294, 16322, 16344,
    16362, 16374, 16382, 16384};
static int32_t isin(int d) {
    d %= 360; if (d < 0) d += 360;
    if (d <= 90) return SIN[d];
    if (d <= 180) return SIN[180 - d];
    if (d <= 270) return -SIN[d - 180];
    return -SIN[360 - d];
}

/* ARGB of image pixel (sx, sy), 0 outside; `row` caches one decoded row of a run-length image */
typedef struct hand_src { const uint8_t *img; uint32_t size, w, h, raw, stride; } hand_src;

static uint32_t src_px(const hand_src *s, int sx, int sy) {
    if (sx < 0 || sy < 0 || (uint32_t)sx >= s->w || (uint32_t)sy >= s->h) return 0;
    if (s->raw) {
        const uint8_t *p = s->img + (uint32_t)sy * s->stride + 3u * (uint32_t)sx;
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 8 | p[2];
    }
    /* run-length row: walk to sx */
    uint32_t entry = RD32(s->img + 4 * (uint32_t)sy), at = entry & 0x1FFFFFu, end = at + (entry >> 21), n = 0;
    if (end > s->size) return 0;
    while (at < end) {
        uint32_t run = s->img[at++], k = run & 127u;
        if (!k) return 0;
        if (run & 128u) {
            if (n + k > (uint32_t)sx) return (uint32_t)s->img[at] << 24 | (uint32_t)s->img[at + 1] << 8 | s->img[at + 2];
            at += 3; n += k;
        } else {
            if (n + k > (uint32_t)sx) { at += 3u * ((uint32_t)sx - n); return (uint32_t)s->img[at] << 24 | (uint32_t)s->img[at + 1] << 8 | s->img[at + 2]; }
            at += 3u * k; n += k;
        }
    }
    return 0;
}

static void hand(ctx *c, const el *e, int angle) {
    uint32_t size;
    const uint8_t *img = image(c, e, 0, &size);
    if (!img) return;
    if (e->format != 8 && e->format != 2) { c->bad = 1; return; }
    hand_src s = { img, size, e->w, e->h, 0, (3u * e->w + 3u) & ~3u };
    if (e->format == 8) s.raw = (uint32_t)is_raw(img, size, e);
    else s.raw = 2;   /* format 2 handled below */
    int px = e->count, py = e->p16 | e->p17 << 8;
    int m = px; if (e->w - px > m) m = e->w - px; if (py > m) m = py; if (e->h - py > m) m = e->h - py;
    int half = m + 1;                     /* the box of side 2m + 2 around the pivot */
    int32_t sn = isin(angle), cs = isin(angle + 90);
    int top = e->y - half, bottom = e->y + half;
    if (top < c->b->y0) top = c->b->y0;
    if (bottom > c->b->y0 + c->b->rows) bottom = c->b->y0 + c->b->rows;
    for (int dy = top; dy < bottom; dy++) {
        for (int dx = e->x - half; dx < e->x + half; dx++) {
            if (dx < 0 || dx >= OWF_W) continue;
            /* pixel centre relative to the pivot, in 1/16 px; turn back by the angle */
            int32_t rx = (dx - e->x) * 16 + 8, ry = (dy - e->y) * 16 + 8;
            int32_t ux = (rx * cs + ry * sn) / 16384, uy = (-rx * sn + ry * cs) / 16384;
            int32_t fx = ux + px * 16 - 8, fy = uy + py * 16 - 8;   /* source position, pixel-centre based */
            int32_t ix = fx >> 4, iy = fy >> 4, wx = fx & 15, wy = fy & 15;
            if (ix < -1 || iy < -1 || ix >= (int32_t)e->w || iy >= (int32_t)e->h) continue;
            uint32_t q[4], acc_a = 0, acc_r = 0, acc_g = 0, acc_b = 0;
            int32_t wts[4] = { (16 - wx) * (16 - wy), wx * (16 - wy), (16 - wx) * wy, wx * wy };
            for (int k = 0; k < 4; k++) {
                int sx = ix + (k & 1), sy = iy + (k >> 1);
                if (s.raw == 2) {   /* opaque format 2: decode via the 3-byte walker is wrong, use a 2-byte walk */
                    q[k] = 0;
                    if (sx >= 0 && sy >= 0 && (uint32_t)sx < s.w && (uint32_t)sy < s.h) {
                        uint32_t entry = RD32(img + 4 * (uint32_t)sy), at = entry & 0x1FFFFFu, end = at + (entry >> 21), n = 0;
                        while (at < end && end <= size) {
                            uint32_t run = img[at++], kk = run & 127u;
                            if (!kk) break;
                            if (run & 128u) { if (n + kk > (uint32_t)sx) { q[k] = 0xFF000000u | (uint32_t)img[at] << 8 | img[at + 1]; break; } at += 2; }
                            else { if (n + kk > (uint32_t)sx) { at += 2u * ((uint32_t)sx - n); q[k] = 0xFF000000u | (uint32_t)img[at] << 8 | img[at + 1]; break; } at += 2u * kk; }
                            n += kk;
                        }
                    }
                } else q[k] = src_px(&s, sx, sy);
                uint32_t a = q[k] >> 24, v = q[k] & 0xFFFFu;
                acc_a += a * (uint32_t)wts[k];
                acc_r += c5(v >> 11, 5) * a * (uint32_t)wts[k];
                acc_g += c5((v >> 5) & 63u, 6) * a * (uint32_t)wts[k];
                acc_b += c5(v & 31u, 5) * a * (uint32_t)wts[k];
            }
            if (!acc_a) continue;
            uint32_t r = acc_r / acc_a, g = acc_g / acc_a, b = acc_b / acc_a, a = (acc_a + 128u) / 256u;
            if (a > 255u) a = 255u;
            plot(c, dx, dy, (r >> 3) << 11 | (g >> 2) << 5 | b >> 3, a);
        }
    }
}

/* ---- numbers ---- */

static int digits(uint32_t v, uint8_t *out) {   /* decimal digits, most significant first */
    uint8_t t[10]; int n = 0;
    do { t[n++] = (uint8_t)(v % 10u); v /= 10u; } while (v && n < 10);
    for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
    return n;
}

static void number(ctx *c, const el *e, const uint8_t *d, int n) {
    int total = e->count * (n - 1) + e->w;
    int x = e->x - (e->align == 1 ? total / 2 : e->align == 2 ? total : 0), y = e->y;
    for (int i = 0; i < n; i++) { put(c, e, d[i], x, y); x += e->count; y += (int8_t)e->p16; }
}

static void two(ctx *c, const el *e, const owf_values *v, uint32_t value) {
    if ((e->type == 0x32 || e->type == 0x33 || e->type == 0x3b) && e->p17 && !e->p16) {   /* ten digits per style */
        uint32_t base = 10u * (v->style % e->p17);
        put(c, e, base + value / 10u % 10u, e->x, e->y);
        put(c, e, base + value % 10u, e->x + e->count, e->y);
        return;
    }
    put(c, e, value / 10u % 10u, e->x, e->y);
    put(c, e, value % 10u, e->x + e->count, e->y + (int8_t)e->p16);
}

static uint32_t level(const el *e, uint32_t value, uint32_t goal) {
    uint32_t n = e->count ? e->count : 1u;
    if (!goal) return 0;
    uint32_t l = value * (n - 1u) / goal;
    return l < n - 1u ? l : n - 1u;
}

/* ---- drawing ---- */

int owf_render(const uint8_t *f, uint32_t len, const owf_values *v, const owf_band *band, uint64_t *skipped) {
    if (skipped) *skipped = 0;
    if (owf_check(f, len)) return -1;
    ctx c = { f, len, band, 0 };
    uint32_t n = f[0] > OWF_MAX_ELEMENTS ? OWF_MAX_ELEMENTS : f[0];
    uint32_t hour = v->hour, shown = v->h12 ? (hour % 12u ? hour % 12u : 12u) : hour;
    int missed = 0;
    uint8_t d[16];
    for (uint32_t i = 0; i < n; i++) {
        el e; load(f + 4 + 24 * i, &e);
        if (e.type == 0xb4 || (e.flags & 2u) || e.flags == 1u) continue;
        c.bad = 0;
        int known = 1, k;
        switch (e.type) {
        case 0x01: {   /* variant image: day/night (+23 = 1, one image per variant) or language (+23 = 1) */
            uint32_t idx = e.p17 == 1 && e.count == 1 ? (hour < 6u || hour >= 18u) : e.p17 == 1 ? (v->lang ? 1u : 0u) : 0u;
            put(&c, &e, idx, e.x, e.y);
            break;
        }
        case 0x02: { uint32_t t = e.p16 | e.p17 << 8; put(&c, &e, (v->ms / (t ? t : 1u)) % (e.count ? e.count : 1u), e.x, e.y); break; }
        case 0x03: put(&c, &e, v->style, e.x, e.y); break;
        case 0x32: two(&c, &e, v, shown); break;
        case 0x33: two(&c, &e, v, v->minute); break;
        case 0x3b: two(&c, &e, v, v->second); break;
        case 0x35: two(&c, &e, v, v->day); break;
        case 0x34:
            if (!e.align) two(&c, &e, v, v->month);
            else put(&c, &e, v->month + (e.p17 == 1 && v->lang ? 12u : 0u), e.x, e.y);
            break;
        case 0x3c: k = digits(v->year, d); number(&c, &e, d, k); break;
        case 0x36: hand(&c, &e, (int)(hour % 12u) * 30 + v->minute / 2); break;
        case 0x37: hand(&c, &e, v->minute * 6 + v->second / 10); break;
        case 0x38: case 0x39: case 0x3d: hand(&c, &e, v->second * 6); break;
        case 0x42: hand(&c, &e, (int)(hour * 60u + v->minute) / 4); break;
        case 0x44: hand(&c, &e, (int)(v->day * 360u / 31u % 360u)); break;
        case 0x3a: put(&c, &e, e.align ? hour : hour % 12u, e.x, e.y); break;
        case 0x3e: put(&c, &e, shown / 10u, e.x, e.y); break;
        case 0x3f: put(&c, &e, shown % 10u, e.x, e.y); break;
        case 0x40: put(&c, &e, v->minute / 10u, e.x, e.y); break;
        case 0x41: put(&c, &e, v->minute % 10u, e.x, e.y); break;
        case 0x63: case 0x64: put(&c, &e, v->weekday + (e.p17 ? 7u * v->lang : 0u), e.x, e.y); break;
        case 0x65:
            if (e.count == 1) put(&c, &e, v->bluetooth, e.x, e.y);
            else if (v->bluetooth) put(&c, &e, 0, e.x, e.y);
            break;
        case 0x66: { uint32_t steps = e.count ? e.count : 1u, l = v->battery / (100u / steps); put(&c, &e, l < steps - 1u ? l : steps - 1u, e.x, e.y); break; }
        case 0x67: if (v->h12) put(&c, &e, hour > 11u, e.x, e.y); break;
        case 0x68: k = digits(v->battery, d); number(&c, &e, d, k); break;
        case 0x6a: put(&c, &e, v->weather, e.x, e.y); break;
        case 0x6b: case 0x6d: {
            int t = v->temperature, m = 0;
            if (t < 0) { d[m++] = 10; t = -t; }
            m += digits((uint32_t)t, d + m);
            if (e.type == 0x6d) d[m++] = 11;
            number(&c, &e, d, m); break;
        }
        case 0x6c: put(&c, &e, 0, e.x, e.y); break;
        case 0x94: k = digits((uint32_t)(((360 - v->compass) % 360 + 360) % 360), d); number(&c, &e, d, k); break;
        case 0x95: put(&c, &e, (uint32_t)((((0x17e - v->compass) % 360) + 360) % 360) / 45u, e.x, e.y); break;
        case 0x96: k = digits(v->steps, d); number(&c, &e, d, k); break;
        case 0x97: k = digits(v->kcal, d); number(&c, &e, d, k); break;
        case 0x98: k = digits(v->heart, d); number(&c, &e, d, k); break;
        case 0x9e: k = digits(v->spo2, d); number(&c, &e, d, k); break;
        case 0x99: {
            uint32_t km = (v->distance_m + 5u) / 10u;
            k = digits(km / 100u, d); d[k++] = 10; d[k++] = (uint8_t)(km % 100u / 10u); d[k++] = (uint8_t)(km % 10u);
            number(&c, &e, d, k); break;
        }
        case 0x9a: two(&c, &e, v, v->sleep_min / 60u); break;
        case 0x9b: two(&c, &e, v, v->sleep_min % 60u); break;
        case 0x9c: if (v->unread) put(&c, &e, v->unread - 1u, e.x, e.y); break;
        case 0xa0: put(&c, &e, level(&e, v->steps, v->step_goal), e.x, e.y); break;
        case 0xa1: put(&c, &e, level(&e, v->kcal, v->kcal_goal), e.x, e.y); break;
        case 0xa3: put(&c, &e, level(&e, v->heart, 200), e.x, e.y); break;
        case 0xa4: put(&c, &e, v->spo2 >= 90u ? level(&e, v->spo2, 100) : 0u, e.x, e.y); break;
        default: known = 0; break;
        }
        if (!known || c.bad) { missed++; if (skipped && i < 64u) *skipped |= (uint64_t)1 << i; }
    }
    return missed;
}

int owf_tap(const uint8_t *f, uint32_t len, int x, int y) {
    if (owf_check(f, len)) return 0;
    uint32_t n = f[0] > OWF_MAX_ELEMENTS ? OWF_MAX_ELEMENTS : f[0];
    for (uint32_t i = 0; i < n; i++) {
        el e; load(f + 4 + 24 * i, &e);
        if (e.type == 0xb4 && x >= e.x && y >= e.y && x <= (int)e.w && y <= (int)e.h) return e.count;   /* w/h = right/bottom */
    }
    return 0;
}

uint32_t owf_period(const uint8_t *f, uint32_t len) {
    if (owf_check(f, len)) return 60000u;
    uint32_t n = f[0] > OWF_MAX_ELEMENTS ? OWF_MAX_ELEMENTS : f[0], p = 60000u;
    for (uint32_t i = 0; i < n; i++) {
        el e; load(f + 4 + 24 * i, &e);
        uint32_t q = 60000u;
        if (e.type == 0x3b || e.type == 0x38) q = 500u;
        else if (e.type == 0x39 || e.type == 0x3d) q = 167u;
        else if (e.type == 0x02) { q = e.p16 | e.p17 << 8; if (!q) q = 100u; }
        else if (e.type == 0x33 || e.type == 0x37 || e.type == 0x41) q = 1000u;
        if (q < p) p = q;
    }
    return p;
}
