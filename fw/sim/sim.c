/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS host simulator: runs the LVGL interface (fw/ui) without a window and writes every tile as a PPM
 * picture. The display works like on the watch: a draw buffer of 368 x 40 rows, flushed as RGB565 high byte first.
 *   sim FACE OUTDIR
 */
#include "lvgl.h"
#include "../ui/ow_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t screen[368 * 448];   /* what the panel would get: RGB565, high byte first */

static void flush(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
    uint32_t w = (uint32_t)lv_area_get_width(a);
    lv_draw_sw_rgb565_swap(px, w * (uint32_t)lv_area_get_height(a));   /* as the watch's flush does for the panel */
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&screen[y * 368 + a->x1], px + (size_t)(y - a->y1) * w * 2, w * 2);
    lv_display_flush_ready(disp);
}

static void save(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n368 448\n255\n");
    for (int i = 0; i < 368 * 448; i++) {
        uint16_t v = screen[i];
        v = (uint16_t)(v >> 8 | v << 8);   /* stored high byte first */
        uint8_t rgb[3] = { (uint8_t)((v >> 11) * 255 / 31), (uint8_t)(((v >> 5) & 63) * 255 / 63), (uint8_t)((v & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: sim FACE OUTDIR\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *face = malloc((size_t)len);
    if (!face || fread(face, 1, (size_t)len, f) != (size_t)len) return 1;
    fclose(f);

    lv_init();
    lv_display_t *disp = lv_display_create(368, 448);
    static uint8_t buf[368 * 40 * 2];   /* draw buffer of 40 rows, as on the watch */
    lv_display_set_buffers(disp, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush);

    static ow_state s = {
        .v = { .hour = 10, .minute = 8, .second = 34, .day = 20, .month = 11, .year = 2026, .weekday = 4, .battery = 80,
               .bluetooth = 1, .weather = 1, .lang = 1, .temperature = 7, .steps = 6540, .kcal = 312, .heart = 72,
               .distance_m = 4200, .sleep_min = 437, .step_goal = 10000, .kcal_goal = 500, .spo2 = 98 },
        .bluetooth_on = 1, .aod = 1, .brightness = 60, .notes = 2,
        .note_app = { "WhatsApp", "Kalender" },
        .note_text = { "Jan: Läuft die neue Oberfläche schon auf der Uhr?", "Morgen 9:00 Zahnarzt" },
    };
    s.face = face; s.face_len = (uint32_t)len;
    ow_ui_create(&s);
    static const char *const names[5] = { "activity", "face", "apps", "control", "notes" };
    for (int t = 0; t < 5; t++) {
        ow_ui_show(t);
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        save(argv[2], names[t]);
    }
    return 0;
}
