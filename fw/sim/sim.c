/* SPDX-License-Identifier: GPL-2.0-only */
/* openWatchOS host simulator: runs the firmware's interface on the PC without a window. A scripted finger taps and
 * swipes, a simulated phone answers (store catalogue and packages from OW_STORE, weather, music), and every step
 * is written as a PPM picture. The display works like on the watch: draw buffer of 368 x 40 rows, flushed as
 * RGB565 high byte first.
 *   OW_ROOT=storage OW_STORE=store sim OUTDIR
 */
#include "lvgl.h"
#include "ow_sim.h"
#include "../apps/ow_app.h"
#include "../sys/ow_sys.h"
#include "../ui/ow_ui.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ow_store_catalog(const char *json, unsigned len);
void ow_store_package(const char *id, const uint8_t *tar, unsigned len);
int ow_alarm_due(uint8_t hour, uint8_t minute);

static uint16_t screen[368 * 448];   /* what the panel would get: RGB565, high byte first */
static const char *outdir;
static int shot_no;

void sim_log(const char *fmt, ...) {
    va_list a; va_start(a, fmt);
    printf("  ");
    vprintf(fmt, a);
    printf("\n");
    va_end(a);
}

/* ---- the simulated phone ---- */

static char pending_channel[32], pending_text[96];

static void *load_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    void *d = malloc((size_t)*len + 1);
    if (d && fread(d, 1, (size_t)*len, f) != (size_t)*len) { free(d); d = 0; }
    fclose(f);
    return d;
}

void sim_phone(const char *channel, const char *text, unsigned len) {   /* answered a moment later, like Bluetooth */
    snprintf(pending_channel, sizeof pending_channel, "%s", channel);
    snprintf(pending_text, sizeof pending_text, "%.*s", (int)len, text);
}

static void phone_answer(void) {
    if (!pending_channel[0]) return;
    const char *store = getenv("OW_STORE") ? getenv("OW_STORE") : "store";
    char path[512]; long len;
    if (!strcmp(pending_channel, "store") && !strcmp(pending_text, "catalog")) {
        snprintf(path, sizeof path, "%s/catalog.json", store);
        char *j = load_file(path, &len);
        if (j) { ow_store_catalog(j, (unsigned)len); free(j); }
    } else if (!strcmp(pending_channel, "store") && !strncmp(pending_text, "get ", 4)) {
        snprintf(path, sizeof path, "%s/%s.oapps", store, pending_text + 4);
        uint8_t *t = load_file(path, &len);
        if (t) { ow_store_package(pending_text + 4, t, (unsigned)len); free(t); }
    }
    pending_channel[0] = 0;
}

/* ---- display, finger, time ---- */

static void flush(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
    uint32_t w = (uint32_t)lv_area_get_width(a);
    lv_draw_sw_rgb565_swap(px, w * (uint32_t)lv_area_get_height(a));   /* as the watch's flush does for the panel */
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&screen[y * 368 + a->x1], px + (size_t)(y - a->y1) * w * 2, w * 2);
    lv_display_flush_ready(disp);
}

static struct { int x, y, pressed; } finger;
static void read_finger(lv_indev_t *in, lv_indev_data_t *d) {
    LV_UNUSED(in);
    d->point.x = finger.x; d->point.y = finger.y;
    d->state = finger.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static uint32_t tick_cb(void) { return ow_sys_ms(); }

static void run(uint32_t ms) {   /* let the firmware run: LVGL every 10 ms, apps every 100 ms, clock every second */
    for (uint32_t t = 0; t < ms; t += 10) {
        ow_sim_advance(10);
        lv_timer_handler();
        uint32_t now = ow_sys_ms();
        if (now % 100 == 0) { ow_app_tick(); phone_answer(); }
        if (now % 1000 == 0) ow_ui_update();
    }
}

static void tap(int x, int y) {
    finger.x = x; finger.y = y; finger.pressed = 1; run(60);
    finger.pressed = 0; run(300);
}

static lv_obj_t *find_text(lv_obj_t *o, const char *text) {   /* a visible label containing `text` */
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), text) && !lv_obj_is_hidden(o)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *r = find_text(lv_obj_get_child(o, (int32_t)i), text);
        if (r) return r;
    }
    return 0;
}

static void tap_text(const char *text) {   /* taps the middle of the label's clickable parent, like a finger */
    lv_obj_t *l = find_text(lv_layer_top(), text);
    if (!l) l = find_text(lv_screen_active(), text);
    if (!l) { printf("  (no \"%s\" on screen)\n", text); return; }
    lv_obj_scroll_to_view_recursive(l, LV_ANIM_OFF);
    run(50);
    lv_area_t a;
    lv_obj_get_coords(l, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}

static void swipe(int x0, int y0, int x1, int y1) {
    finger.x = x0; finger.y = y0; finger.pressed = 1; run(30);
    for (int i = 1; i <= 8; i++) { finger.x = x0 + (x1 - x0) * i / 8; finger.y = y0 + (y1 - y0) * i / 8; run(20); }
    finger.pressed = 0; run(700);
}

static void shot(const char *name) {
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(0);
    char path[512];
    snprintf(path, sizeof path, "%s/%02d_%s.ppm", outdir, ++shot_no, name);
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
    printf("%02d %s\n", shot_no, name);
}

static void home(void) { while (ow_app_depth()) ow_app_back(); run(100); ow_ui_show(OW_TILE_FACE); run(100); }
static void open(const char *id) { ow_app_open(ow_app_find(id), 0); run(300); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: OW_ROOT=storage OW_STORE=store sim OUTDIR\n"); return 2; }
    outdir = argv[1];
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(368, 448);
    static uint8_t buf[368 * 40 * 2];   /* draw buffer of 40 rows, as on the watch */
    lv_display_set_buffers(disp, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, read_finger);

    static ow_state s = {
        .v = { .lang = 1, .temperature = 7, .weather = 1, .bluetooth = 1, .steps = 6540, .kcal = 312, .heart = 72,
               .distance_m = 4200, .sleep_min = 437, .step_goal = 10000, .kcal_goal = 500, .spo2 = 98 },
        .bluetooth_on = 1, .notes = 2,
        .note_app = { "WhatsApp", "Kalender" },
        .note_text = { "Jan: Läuft die neue Oberfläche schon auf der Uhr?", "Morgen 9:00 Zahnarzt" },
        .weather = { .temp = 7, .low = 3, .high = 9, .code = 1, .city = "Hamburg", .valid = 1,
                     .days = { { 2, 8, 3 }, { 1, 6, 2 }, { -1, 4, 4 } } },
        .music = { .title = "Weit weg", .artist = "Die Band aus dem Simulator", .playing = 1, .volume = 60 },
        .heart_hours = { 58, 56, 55, 54, 55, 57, 62, 75, 82, 79, 77, 74, 88, 92, 76, 73, 71, 80, 112, 98, 78, 70, 64, 60 },
    };
    ow_ui_create(&s);
    run(500);

    shot("zifferblatt");
    swipe(300, 224, 60, 224); shot("apps");
    ow_ui_show(OW_TILE_FACE); run(100);
    swipe(184, 60, 184, 400); shot("kontrollzentrum");
    ow_ui_show(OW_TILE_FACE); run(100);
    swipe(60, 224, 320, 224); shot("aktivitaet");
    ow_ui_show(OW_TILE_FACE); run(100);
    swipe(184, 400, 184, 60); shot("mitteilungen");
    home();

    /* app store: catalogue from the phone, install an app, open it */
    open("store"); shot("store_leer");
    tap(334, 94); run(500); shot("store_katalog");
    tap_text("Hallo"); run(300); shot("store_detail");
    tap_text("Installieren"); run(800); shot("store_installiert");
    tap_text("Öffnen"); run(500); shot("js_app");
    tap(200, 160); run(300); tap(120, 330); run(300); shot("js_app_getippt");
    ow_app_back(); run(300);
    tap_text("Im Menü"); run(100);
    { lv_obj_t *l = find_text(lv_layer_top(), "Im Menü"); if (l) { lv_obj_t *sw = lv_obj_get_child(lv_obj_get_parent(l), 2); lv_obj_add_state(sw, LV_STATE_CHECKED); lv_obj_send_event(sw, LV_EVENT_VALUE_CHANGED, 0); } }
    run(200); shot("store_im_menue");
    ow_app_back(); run(200);
    tap_text("Uhr"); run(300); tap_text("Installieren"); run(800); tap_text("Öffnen"); run(1500); shot("js_uhr");
    home();
    open("jsapps"); shot("meine_apps");
    home();

    open("stopwatch"); tap(260, 360); run(3270); tap(108, 360); run(1230); shot("stoppuhr");
    home();
    open("timer"); tap(186, 140); run(4000); shot("timer");
    home();
    open("alarm"); shot("wecker");
    home();
    open("weather"); shot("wetter"); home();
    open("music"); shot("musik"); home();
    open("heart"); shot("puls"); home();
    open("faces"); shot("zifferblaetter");
    tap(334, 182); run(400); shot("zifferblaetter_2"); home();
    open("settings"); shot("einstellungen");
    swipe(184, 380, 184, 60); shot("einstellungen_2");
    home();
    swipe(300, 224, 60, 224); swipe(184, 400, 184, 0); swipe(184, 400, 184, 0); shot("apps_ende");
    tap_text("Hallo"); run(500); shot("js_aus_menue");
    printf("alarm due 06:45: %d\n", ow_alarm_due(6, 45));
    return 0;
}
