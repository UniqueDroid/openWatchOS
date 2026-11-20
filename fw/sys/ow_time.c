/* SPDX-License-Identifier: GPL-2.0-only */
#include "ow_sys.h"

void ow_time_split(uint32_t s, ow_time *t) {
    uint32_t days = s / 86400u, rest = s % 86400u, year = 2000u, month = 0, n;
    t->weekday = (uint8_t)((days + 5u) % 7u);   /* 2000-01-01 was a Saturday */
    for (;;) { n = (year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ? 366u : 365u; if (days < n) break; days -= n; year++; }
    for (;;) {
        int leap = year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u);
        n = month == 1u ? (leap ? 29u : 28u) : (month == 3u || month == 5u || month == 8u || month == 10u) ? 30u : 31u;
        if (days < n) break;
        days -= n; month++;
    }
    t->year = (uint16_t)year; t->month = (uint8_t)(month + 1u); t->day = (uint8_t)(days + 1u);
    t->hour = (uint8_t)(rest / 3600u); t->minute = (uint8_t)(rest / 60u % 60u); t->second = (uint8_t)(rest % 60u);
}
