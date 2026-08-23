// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Host implementation of panel_hal.h for tests (and a template for the
// Emscripten host): a memory bus instead of a UART, a fake clock, a RAM store.
#ifndef HAL_HOST_H
#define HAL_HOST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void hal_host_reset(void);
void hal_host_advance_ms(uint32_t ms);
void hal_host_set_buttons_mv(uint16_t mv);

// The machine side of the memory bus. The panel side is hal_bus_write/read.
size_t membus_machine_read(uint8_t *buf, size_t max);
size_t membus_machine_write(const uint8_t *buf, size_t len);

const uint16_t *hal_host_framebuffer(void);   // HAL_DISPLAY_W * HAL_DISPLAY_H
void hal_host_leds(bool *pwr, bool *bus, bool *fault);
uint8_t hal_host_backlight(void);
#endif
