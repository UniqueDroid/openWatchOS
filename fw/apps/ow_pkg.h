/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_PKG_H
#define OW_PKG_H
/* Installed packages (.oapps, see the JS app format): kept as files in apps/<id>/.
 * install: checks the tar archive and its app.json, then writes the files. The store and the phone use it. */
#include <stdint.h>

#define OW_PKG_MAX 32
#define OW_PKG_MAX_BYTES (512u * 1024u)

typedef struct ow_pkg_info {
    char id[48], name[24], version[16], type[12], author[24];
    char description[96];
    uint32_t permissions;     /* bit per permission, order as in ow_pkg_permission_names */
    uint32_t colour;
    uint32_t bytes;           /* on the watch */
    uint8_t pinned;           /* shown in the main app list */
} ow_pkg_info;

enum { OW_PKG_OK = 0, OW_PKG_NOT_TAR = -1, OW_PKG_NO_APPJSON = -2, OW_PKG_BAD_APPJSON = -3, OW_PKG_BAD_PATH = -4,
       OW_PKG_TOO_BIG = -5, OW_PKG_WRITE = -6, OW_PKG_NO_SCRIPT = -7 };

extern const char *const ow_pkg_permission_names[16];

int ow_pkg_install(const uint8_t *tar, uint32_t len, ow_pkg_info *out);
int ow_pkg_remove(const char *id);
int ow_pkg_list(ow_pkg_info *out, int max);            /* sorted by name */
int ow_pkg_get(const char *id, ow_pkg_info *out);       /* 0 = installed */
int ow_pkg_pin(const char *id, int on);
/* compares "1.2.3" versions: <0, 0, >0 */
int ow_pkg_version_cmp(const char *a, const char *b);
const char *ow_pkg_error(int code);                     /* German text for the user */

#endif
