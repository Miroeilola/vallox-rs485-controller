// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser host's side of panel_hal.h: what sim.c and the tests poke.
// Mirrors hal_host.h from the host tests; the ladder levels are spec §3.1.
#ifndef HAL_WEB_H
#define HAL_WEB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HAL_WEB_LADDER_NONE_MV 3300u
#define HAL_WEB_LADDER_SW1_MV     0u
#define HAL_WEB_LADDER_SW2_MV   430u
#define HAL_WEB_LADDER_SW3_MV   819u
#define HAL_WEB_LADDER_SW4_MV  1336u

#define HAL_WEB_MIN_HOLD_SAMPLES 3   // a press is sampled at least this often before a release counts
#define HAL_WEB_STORE_MAX       16
#define HAL_WEB_LOG_MAX         256
#define HAL_WEB_LOG_ENTRY_BYTES 12

void     hal_web_reset(void);                 // everything but the store
void     hal_web_set_time_ms(uint32_t now_ms);
void     hal_web_set_button(int idx, bool down);   // 0..3 = SW1..SW4
uint16_t hal_web_button_mv(void);

// The machine's end of the memory bus (the panel's end is hal_bus_write/read).
size_t   hal_web_machine_read(uint8_t *buf, size_t max);
size_t   hal_web_machine_write(const uint8_t *buf, size_t len);

const uint8_t *hal_web_rgba(void);            // HAL_DISPLAY_W*HAL_DISPLAY_H*4
bool     hal_web_take_dirty(int *x, int *y, int *w, int *h);
int      hal_web_flushes(void);
uint8_t  hal_web_leds(void);                  // bit0 pwr, bit1 bus, bit2 fault
uint8_t  hal_web_backlight(void);

int      hal_web_log_total(void);
const uint8_t *hal_web_log_entry(int seq);    // NULL if seq left the ring

int      hal_web_store_count(void);
const char *hal_web_store_key(int i);
const uint8_t *hal_web_store_value(int i, int *len);
bool     hal_web_store_take_dirty(void);
#endif
