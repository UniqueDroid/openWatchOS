/* SPDX-License-Identifier: GPL-2.0-only */
/* Platform layer on the PC: storage is a folder (OW_ROOT, default "storage"), time and the phone link are
 * simulated so that every run gives the same pictures. */
#define _DEFAULT_SOURCE
#include "../sys/ow_sys.h"
#include "ow_sim.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint32_t now_ms, seconds = 848484514u;   /* 2026-11-20 10:08:34 */
static const char *root(void) { const char *r = getenv("OW_ROOT"); return r ? r : "storage"; }
static void full(char *out, size_t cap, const char *path) { snprintf(out, cap, "%s/%s", root(), path); }

uint32_t ow_sys_ms(void) { return now_ms; }
uint32_t ow_sys_seconds(void) { return seconds + now_ms / 1000u; }
void ow_sys_set_seconds(uint32_t s) { seconds = s - now_ms / 1000u; }
void ow_sim_advance(uint32_t ms) { now_ms += ms; }
void ow_sys_vibrate(unsigned ms) { sim_log("vibrate %u ms", ms); }
void ow_sys_brightness(unsigned p) { sim_log("brightness %u %%", p); }
unsigned ow_sys_battery(void) { return 80; }

int ow_sys_read(const char *path, void *buf, unsigned cap) {
    char p[512]; full(p, sizeof p, path);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return (int)n;
}

int ow_sys_size(const char *path) {
    char p[512]; full(p, sizeof p, path);
    struct stat s;
    return stat(p, &s) ? -1 : (int)s.st_size;
}

static void make_dirs(char *p) {
    for (char *q = strchr(p + 1, '/'); q; q = strchr(q + 1, '/')) { *q = 0; mkdir(p, 0755); *q = '/'; }
}

int ow_sys_write(const char *path, const void *buf, unsigned len) {
    char p[512]; full(p, sizeof p, path);
    make_dirs(p);
    FILE *f = fopen(p, "wb");
    if (!f) return -1;
    int ok = fwrite(buf, 1, len, f) == len;
    return fclose(f) == 0 && ok ? 0 : -1;
}

static int remove_tree(const char *p) {
    struct stat s;
    if (stat(p, &s)) return -1;
    if (S_ISDIR(s.st_mode)) {
        DIR *d = opendir(p);
        struct dirent *e;
        while (d && (e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char q[768]; snprintf(q, sizeof q, "%s/%s", p, e->d_name);
            remove_tree(q);
        }
        if (d) closedir(d);
        return rmdir(p);
    }
    return unlink(p);
}

int ow_sys_remove(const char *path) { char p[512]; full(p, sizeof p, path); return remove_tree(p); }

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int ow_sys_list(const char *dir, void (*fn)(const char *, int, void *), void *user) {
    char p[512]; full(p, sizeof p, dir);
    DIR *d = opendir(p);
    if (!d) return 0;
    char *names[256]; int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 256)
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) names[n++] = strdup(e->d_name);
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], by_name);   /* same order on every run */
    for (int i = 0; i < n; i++) {
        char q[768]; struct stat s;
        snprintf(q, sizeof q, "%s/%s", p, names[i]);
        fn(names[i], !stat(q, &s) && S_ISDIR(s.st_mode), user);
        free(names[i]);
    }
    return n;
}

int ow_sys_connected(void) { return 1; }

int ow_sys_send(const char *channel, const char *text, unsigned len) {
    sim_log("to phone [%s] %.*s", channel, (int)len, text);
    sim_phone(channel, text, len);
    return 1;
}
