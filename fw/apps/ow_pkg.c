/* SPDX-License-Identifier: GPL-2.0-only */
#include "ow_pkg.h"
#include "ow_json.h"
#include "../sys/ow_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const ow_pkg_permission_names[16] = { "time", "timer", "vibrate", "keepawake", "storage", "battery",
    "sensor.steps", "sensor.heart", "phone.http", "notify", "phone.message", "microphone", "debug", "boot", 0, 0 };

const char *ow_pkg_error(int code) {
    switch (code) {
    case OW_PKG_OK: return "Installiert";
    case OW_PKG_NOT_TAR: return "Kein gültiges Paket";
    case OW_PKG_NO_APPJSON: return "app.json fehlt";
    case OW_PKG_BAD_APPJSON: return "app.json ist fehlerhaft";
    case OW_PKG_BAD_PATH: return "Unzulässiger Dateiname im Paket";
    case OW_PKG_TOO_BIG: return "Paket zu groß";
    case OW_PKG_WRITE: return "Speichern fehlgeschlagen";
    case OW_PKG_NO_SCRIPT: return "Skript fehlt";
    default: return "Fehler";
    }
}

/* ---- tar (ustar): 512-byte headers, octal sizes ---- */

typedef struct entry { char name[101]; const uint8_t *data; uint32_t size; char type; } entry;

static uint32_t octal(const uint8_t *p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n && p[i]; i++) if (p[i] >= '0' && p[i] <= '7') v = v * 8u + (uint32_t)(p[i] - '0');
    return v;
}

/* next entry after `at`; returns the offset after it, 0 at the end, -1 if broken */
static long tar_next(const uint8_t *t, uint32_t len, uint32_t at, entry *e) {
    if (at + 512u > len) return -1;
    const uint8_t *h = t + at;
    int empty = 1;
    for (int i = 0; i < 512; i++) if (h[i]) { empty = 0; break; }
    if (empty) return 0;
    uint32_t sum = 0, want = octal(h + 148, 8);
    for (int i = 0; i < 512; i++) sum += (i >= 148 && i < 156) ? 32u : h[i];
    if (sum != want) return -1;
    memcpy(e->name, h, 100); e->name[100] = 0;
    if (h[345]) {   /* ustar prefix */
        char full[256]; snprintf(full, sizeof full, "%.155s/%s", (const char *)h + 345, e->name);
        snprintf(e->name, sizeof e->name, "%s", full);
    }
    e->size = octal(h + 124, 12);
    e->type = (char)(h[156] ? h[156] : '0');
    e->data = h + 512;
    uint32_t next = at + 512u + ((e->size + 511u) & ~511u);
    if (next > len || at + 512u + e->size > len) return -1;
    return (long)next;
}

static int safe_name(const char *n) {
    if (!n[0] || n[0] == '/' || strstr(n, "..") || strchr(n, '\\') || strlen(n) > 64) return 0;
    for (const char *p = n; *p; p++) if ((unsigned char)*p < 32) return 0;
    return 1;
}

static uint32_t colour_of(const char *id) {   /* a stable colour per app */
    static const uint32_t C[8] = { 0x0A84FF, 0x30D158, 0xFF9F0A, 0xFF375F, 0xBF5AF2, 0x64D2FF, 0xFFD60A, 0xAC8E68 };
    uint32_t h = 2166136261u;
    for (; *id; id++) h = (h ^ (uint8_t)*id) * 16777619u;
    return C[h % 8u];
}

static int parse_appjson(const char *j, const char *end, ow_pkg_info *o) {
    memset(o, 0, sizeof *o);
    if (ow_json_str(ow_json_get(j, end, "id"), end, o->id, sizeof o->id) <= 0) return OW_PKG_BAD_APPJSON;
    for (const char *p = o->id; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return OW_PKG_BAD_APPJSON;
    if (!strchr(o->id, '.')) return OW_PKG_BAD_APPJSON;
    if (ow_json_str(ow_json_get(j, end, "name"), end, o->name, sizeof o->name) <= 0) return OW_PKG_BAD_APPJSON;
    if (ow_json_str(ow_json_get(j, end, "version"), end, o->version, sizeof o->version) <= 0) return OW_PKG_BAD_APPJSON;
    if (ow_json_str(ow_json_get(j, end, "type"), end, o->type, sizeof o->type) <= 0) strcpy(o->type, "app");
    ow_json_str(ow_json_get(j, end, "author"), end, o->author, sizeof o->author);
    ow_json_str(ow_json_get(j, end, "description"), end, o->description, sizeof o->description);
    const char *perm = ow_json_get(j, end, "permissions");
    for (const char *e = perm ? ow_json_first(perm, end) : 0; e; e = ow_json_next(e, end)) {
        char name[24];
        if (ow_json_str(e, end, name, sizeof name) < 0) return OW_PKG_BAD_APPJSON;
        int k = 0;
        while (k < 16 && !(ow_pkg_permission_names[k] && !strcmp(ow_pkg_permission_names[k], name))) k++;
        if (k == 16) return OW_PKG_BAD_APPJSON;   /* unknown permission: refuse, as the packer does */
        o->permissions |= 1u << k;
    }
    o->colour = colour_of(o->id);
    return OW_PKG_OK;
}

static int pinned(const char *id) {
    char buf[1024];
    int n = ow_sys_read("sys/pinned.txt", buf, sizeof buf - 1);
    if (n <= 0) return 0;
    buf[n] = 0;
    for (char *l = strtok(buf, "\n"); l; l = strtok(0, "\n")) if (!strcmp(l, id)) return 1;
    return 0;
}

int ow_pkg_pin(const char *id, int on) {
    char buf[1024], out[1100];
    int n = ow_sys_read("sys/pinned.txt", buf, sizeof buf - 1), k = 0;
    buf[n > 0 ? n : 0] = 0;
    for (char *l = strtok(buf, "\n"); l; l = strtok(0, "\n"))
        if (strcmp(l, id)) k += snprintf(out + k, sizeof out - (size_t)k, "%s\n", l);
    if (on) k += snprintf(out + k, sizeof out - (size_t)k, "%s\n", id);
    return ow_sys_write("sys/pinned.txt", out, (unsigned)k);
}

int ow_pkg_install(const uint8_t *t, uint32_t len, ow_pkg_info *out) {
    if (len > OW_PKG_MAX_BYTES) return OW_PKG_TOO_BIG;
    entry e; long at = 0, next;
    const entry *appjson = 0; static entry files[24]; int nf = 0, script = 0;
    /* first pass: check everything before writing anything */
    while ((next = tar_next(t, len, (uint32_t)at, &e)) > 0) {
        if (e.type == '0') {
            if (!safe_name(e.name) || nf == 24) return OW_PKG_BAD_PATH;
            files[nf] = e;
            if (!strcmp(e.name, "app.json")) appjson = &files[nf];
            if (!strcmp(e.name, "watch.txt")) script = 1;
            nf++;
        } else if (e.type != '5') return OW_PKG_BAD_PATH;   /* only files and folders */
        at = next;
    }
    if (next < 0 || !nf) return OW_PKG_NOT_TAR;
    if (!appjson) return OW_PKG_NO_APPJSON;
    ow_pkg_info info;
    int r = parse_appjson((const char *)appjson->data, (const char *)appjson->data + appjson->size, &info);
    if (r) return r;
    if (!strcmp(info.type, "app") && !script) return OW_PKG_NO_SCRIPT;
    /* replace an installed version */
    char dir[64], path[160];
    snprintf(dir, sizeof dir, "apps/%s", info.id);
    ow_sys_remove(dir);
    for (int i = 0; i < nf; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i].name);
        if (ow_sys_write(path, files[i].data, files[i].size)) { ow_sys_remove(dir); return OW_PKG_WRITE; }
        info.bytes += files[i].size;
    }
    info.pinned = (uint8_t)pinned(info.id);
    if (out) *out = info;
    return OW_PKG_OK;
}

int ow_pkg_remove(const char *id) {
    char dir[64];
    if (!safe_name(id) || strchr(id, '/')) return -1;
    snprintf(dir, sizeof dir, "apps/%s", id);
    ow_pkg_pin(id, 0);
    return ow_sys_remove(dir);
}

int ow_pkg_get(const char *id, ow_pkg_info *o) {
    char path[128], buf[4096];
    snprintf(path, sizeof path, "apps/%s/app.json", id);
    int n = ow_sys_read(path, buf, sizeof buf);
    if (n <= 0) return -1;
    if (parse_appjson(buf, buf + n, o)) return -1;
    o->pinned = (uint8_t)pinned(o->id);
    return 0;
}

typedef struct lst { ow_pkg_info *out; int max, n; } lst;
static void add(const char *name, int is_dir, void *u) {
    lst *l = u;
    if (is_dir && l->n < l->max && !ow_pkg_get(name, &l->out[l->n])) l->n++;
}
static int by_name(const void *a, const void *b) { return strcmp(((const ow_pkg_info *)a)->name, ((const ow_pkg_info *)b)->name); }

int ow_pkg_list(ow_pkg_info *out, int max) {
    lst l = { out, max, 0 };
    ow_sys_list("apps", add, &l);
    qsort(out, (size_t)l.n, sizeof *out, by_name);
    return l.n;
}

int ow_pkg_version_cmp(const char *a, const char *b) {
    for (int i = 0; i < 3; i++) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return 0;
}
