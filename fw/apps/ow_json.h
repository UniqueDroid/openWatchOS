/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_JSON_H
#define OW_JSON_H
/* Small JSON reader without allocation: finds fields in objects and walks arrays of a text in memory.
 * Enough for app.json and the store catalogue; values are pointers into the text. */

/* value of `key` in the object starting at p (whitespace allowed before it), or 0 */
const char *ow_json_get(const char *p, const char *end, const char *key);
/* string value as UTF-8 into out (always 0-terminated); returns its length, -1 if it is not a string */
int ow_json_str(const char *v, const char *end, char *out, int cap);
/* number value (integer part), `fallback` if it is not a number */
long ow_json_num(const char *v, const char *end, long fallback);
/* 1 for true, 0 for false, `fallback` otherwise */
int ow_json_bool(const char *v, const char *end, int fallback);
/* arrays: first element of the array at v, then the next after an element; 0 at the end */
const char *ow_json_first(const char *v, const char *end);
const char *ow_json_next(const char *element, const char *end);
/* end of the value starting at p (after skipping whitespace), 0 if it is broken */
const char *ow_json_skip(const char *p, const char *end);

#endif
