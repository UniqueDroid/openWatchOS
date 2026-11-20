/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OW_SIM_H
#define OW_SIM_H
#include <stdint.h>
void sim_log(const char *fmt, ...);
void sim_phone(const char *channel, const char *text, unsigned len);   /* the simulated phone answers */
void ow_sim_advance(uint32_t ms);
#endif
