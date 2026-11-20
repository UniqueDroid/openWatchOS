/* SPDX-License-Identifier: GPL-2.0-only */
/* Host test of the package manager and the JSON reader:
 *   cc -I. -o test_pkg apps/test_pkg.c apps/ow_pkg.c apps/ow_json.c sys/ow_time.c sim/ow_sys_sim.c && ./test_pkg PKG.oapps
 * PKG.oapps: any valid JS app package (e.g. hallo.oapps). Uses a temporary storage folder. */
#include "apps/ow_json.h"
#include "apps/ow_pkg.h"
#include "sys/ow_sys.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sim_log(const char *fmt, ...) { (void)fmt; }
void sim_phone(const char *c, const char *t, unsigned l) { (void)c; (void)t; (void)l; }

static uint8_t *file(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); assert(f);
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)*n); assert(fread(d, 1, (size_t)*n, f) == (size_t)*n); fclose(f);
    return d;
}

/* a one-file ustar archive */
static long tar1(uint8_t *out, const char *name, const char *body) {
    memset(out, 0, 2048);
    snprintf((char *)out, 100, "%s", name);
    snprintf((char *)out + 100, 8, "0000644");
    snprintf((char *)out + 124, 12, "%011o", (unsigned)strlen(body));
    out[156] = '0'; memcpy(out + 257, "ustar", 5);
    memset(out + 148, ' ', 8);
    unsigned sum = 0; for (int i = 0; i < 512; i++) sum += out[i];
    snprintf((char *)out + 148, 8, "%06o", sum);
    memcpy(out + 512, body, strlen(body));
    return 2048;
}

int main(int argc, char **argv) {
    char root[] = "/tmp/owtestXXXXXX"; assert(mkdtemp(root)); setenv("OW_ROOT", root, 1);
    /* JSON */
    const char *j = " {\"a\": [1, {\"b\": \"x\\u00e4\\\"y\"}], \"n\": -42, \"t\": true, \"s\": \"ok\"} ";
    const char *end = j + strlen(j); char s[32];
    assert(ow_json_num(ow_json_get(j, end, "n"), end, 0) == -42);
    assert(ow_json_bool(ow_json_get(j, end, "t"), end, 0) == 1);
    const char *a = ow_json_first(ow_json_get(j, end, "a"), end); assert(a && ow_json_num(a, end, 0) == 1);
    const char *b = ow_json_next(a, end); assert(b && ow_json_str(ow_json_get(b, end, "b"), end, s, sizeof s) == 5 && !strcmp(s, "x\xC3\xA4\"y"));
    assert(!ow_json_next(b, end) && !ow_json_get(j, end, "missing"));
    assert(ow_json_str(ow_json_get(j, end, "s"), end, s, sizeof s) == 2);
    /* versions */
    assert(ow_pkg_version_cmp("1.0.9", "1.0.10") < 0 && ow_pkg_version_cmp("2.0.0", "1.9.9") > 0 && !ow_pkg_version_cmp("1.2.3", "1.2.3"));
    /* a real package */
    if (argc > 1) {
        long n; uint8_t *t = file(argv[1], &n); ow_pkg_info i;
        assert(ow_pkg_install(t, (uint32_t)n, &i) == OW_PKG_OK && i.id[0] && i.name[0] && i.bytes > 0);
        ow_pkg_info l[4]; assert(ow_pkg_list(l, 4) == 1 && !strcmp(l[0].id, i.id));
        assert(!ow_pkg_pin(i.id, 1) && !ow_pkg_get(i.id, &l[0]) && l[0].pinned);
        t[100] ^= 1;   /* broken header checksum */
        assert(ow_pkg_install(t, (uint32_t)n, 0) == OW_PKG_NOT_TAR);
        assert(!ow_pkg_remove(i.id) && ow_pkg_list(l, 4) == 0);
        free(t);
    }
    /* refused packages: path outside the app folder, no app.json, unknown permission, no script */
    static uint8_t t[2048];
    assert(ow_pkg_install(t, (uint32_t)tar1(t, "../evil", "x"), 0) == OW_PKG_BAD_PATH);
    assert(ow_pkg_install(t, (uint32_t)tar1(t, "app.js", "function draw(){}"), 0) == OW_PKG_NO_APPJSON);
    assert(ow_pkg_install(t, (uint32_t)tar1(t, "app.json", "{\"id\":\"de.a.b\",\"name\":\"B\",\"version\":\"1.0.0\",\"permissions\":[\"root\"]}"), 0) == OW_PKG_BAD_APPJSON);
    assert(ow_pkg_install(t, (uint32_t)tar1(t, "app.json", "{\"id\":\"de.a.b\",\"name\":\"B\",\"version\":\"1.0.0\"}"), 0) == OW_PKG_NO_SCRIPT);
    assert(ow_pkg_install(t, (uint32_t)tar1(t, "app.json", "{\"id\":\"../x\",\"name\":\"B\",\"version\":\"1.0.0\"}"), 0) == OW_PKG_BAD_APPJSON);
    ow_sys_remove("");
    printf("ok: json, versions, install/list/pin/remove, 6 refused packages\n");
    return 0;
}
