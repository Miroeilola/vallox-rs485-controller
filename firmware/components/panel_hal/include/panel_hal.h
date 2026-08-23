// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The contract between the panel core (panel_ui, vallox_machine in the
// simulator) and whatever hosts it: ESP-IDF, ESPHome, the host test runner, the
// Emscripten build. Plain C functions resolved at link time — one implementation
// per host, none of them inside the core.
//
// The core is tick-driven and single-threaded. Every function here must be safe
// to call from the tick and must not block.

#ifndef PANEL_HAL_H
#define PANEL_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_DISPLAY_W 320
#define HAL_DISPLAY_H 240

// Copy a rectangle of RGB565 pixels (row-major, w*h entries) to the display.
void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint16_t *rgb565);

// Button-ladder voltage in millivolts. The core maps it to a button; the
// thresholds are core code so they are tested on the host.
uint16_t hal_buttons_read_mv(void);

void hal_leds_set(bool pwr, bool bus, bool fault);
void hal_backlight_set(uint8_t level);   // 0 = off, 255 = full

// RS-485 bytes. Driver-enable timing and the idle-gap detection belong to the
// host; the core only sees bytes.
size_t hal_bus_write(const uint8_t *buf, size_t len);   // bytes accepted
size_t hal_bus_read(uint8_t *buf, size_t max);          // bytes copied, 0 if none

uint32_t hal_time_ms(void);   // monotonic, wraps after 49 days — use differences

// Settings. Keys are short ASCII; values opaque bytes. get returns false when
// the key is absent or the stored length differs from len.
bool hal_store_get(const char *key, void *buf, size_t len);
bool hal_store_put(const char *key, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif
