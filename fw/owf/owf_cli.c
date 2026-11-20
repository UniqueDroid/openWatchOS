/* SPDX-License-Identifier: GPL-2.0-only */
/* Host tool: renders a watch face file to a PPM picture, in bands of 40 rows as the firmware does.
 *   owf_cli FACE OUT.ppm [key=value ...]   (keys as in owf_values; defaults below)
 * Prints the number of elements that were not drawn and their indexes.
 */
#include "owf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: owf_cli FACE OUT.ppm [key=value ...]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)len);
    if (!data || fread(data, 1, (size_t)len, f) != (size_t)len) return 1;
    fclose(f);
    owf_values v = { .hour = 10, .minute = 8, .second = 34, .day = 14, .month = 11, .year = 2026, .weekday = 5,
                     .battery = 80, .bluetooth = 1, .unread = 0, .weather = 1, .lang = 1, .h12 = 0, .style = 0,
                     .temperature = 18, .compass = 0, .steps = 6540, .kcal = 312, .heart = 72, .distance_m = 4200,
                     .sleep_min = 437, .step_goal = 10000, .kcal_goal = 500, .spo2 = 98, .ms = 0 };
    for (int i = 3; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) continue;
        *eq = 0; long x = strtol(eq + 1, 0, 10);
#define K(name) if (!strcmp(argv[i], #name)) v.name = x;
        K(hour) K(minute) K(second) K(day) K(month) K(year) K(weekday) K(battery) K(bluetooth) K(unread) K(weather)
        K(lang) K(h12) K(style) K(temperature) K(compass) K(steps) K(kcal) K(heart) K(distance_m) K(sleep_min)
        K(step_goal) K(kcal_goal) K(spo2) K(ms)
#undef K
    }
    static uint16_t screen[OWF_W * OWF_H];
    int missed = 0; uint64_t all = 0;
    for (int y0 = 0; y0 < OWF_H; y0 += 40) {
        owf_band b = { screen + y0 * OWF_W, y0, OWF_H - y0 < 40 ? OWF_H - y0 : 40, 0 };
        uint64_t sk;
        int m = owf_render(data, (uint32_t)len, &v, &b, &sk);
        if (m < 0) { fprintf(stderr, "not a watch face\n"); return 1; }
        if (m > missed) missed = m;
        all |= sk;
    }
    FILE *o = fopen(argv[2], "wb");
    if (!o) { perror(argv[2]); return 1; }
    fprintf(o, "P6\n%d %d\n255\n", OWF_W, OWF_H);
    for (int i = 0; i < OWF_W * OWF_H; i++) {
        uint32_t p = screen[i];
        uint8_t rgb[3] = { (uint8_t)((p >> 11) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)((p & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, o);
    }
    fclose(o);
    printf("not drawn: %d", missed);
    for (int i = 0; i < 64; i++) if (all >> i & 1) printf(" %d", i);
    printf("\n");
    return 0;
}
