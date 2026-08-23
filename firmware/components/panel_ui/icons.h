// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#ifndef PANEL_ICONS_H
#define PANEL_ICONS_H
#include <stdint.h>
// 1-bit icons, 12 × 12: two bytes per row, MSB first, bit 15..4 used (low 4 bits of byte 1 unused).
typedef struct { uint8_t w, h; const uint8_t *rows; } icon_t;
extern const icon_t icon_fault;    // triangle with !
extern const icon_t icon_bus;      // two arrows ⇄
extern const icon_t icon_heater;   // flame
extern const icon_t icon_bypass;   // arrow around a bar
extern const icon_t icon_boost;    // upward chevrons
#endif
