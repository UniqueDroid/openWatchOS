/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS host simulator: runs the LVGL interface (fw/ui) without a window and writes every tile as a PPM
 * picture. The display driver works like on the watch: a draw buffer of 368 x 40 rows, RGB565 high byte first.
 *   sim FACE OUTDIR
 */
#include "lvgl.h"
#include "../ui/ow_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_color_t screen[368 * 448];

static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&screen[y * 368 + a->x1], px + (y - a->y1) * lv_area_get_width(a), lv_area_get_width(a) * sizeof(lv_color_t));
    lv_disp_flush_ready(drv);
}

static void save(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n368 448\n255\n");
    for (int i = 0; i < 368 * 448; i++) {
        uint16_t v = screen[i].full;
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
    static lv_disp_draw_buf_t db;
    static lv_color_t buf[368 * 40];
    lv_disp_draw_buf_init(&db, buf, 0, 368 * 40);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = 368; drv.ver_res = 448; drv.flush_cb = flush; drv.draw_buf = &db;
    lv_disp_drv_register(&drv);

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
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(0);
        save(argv[2], names[t]);
    }
    return 0;
}
