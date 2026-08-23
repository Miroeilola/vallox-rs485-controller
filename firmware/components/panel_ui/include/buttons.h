// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Four buttons on one ADC ladder → debounced PRESS / REPEAT / LONG events.
// Levels from the rev A schematic (simulated 2026-08-23): none 3300 mV,
// SW1 (−) 0, SW2 (+) 430, SW3 (OK) 819, SW4 (←) 1336; thresholds at the
// midpoints. The bring-up measurement replaces the numbers, not the code.
#ifndef PANEL_BUTTONS_H
#define PANEL_BUTTONS_H
#include <stdint.h>

typedef enum { BTN_NONE = 0, BTN_MINUS, BTN_PLUS, BTN_OK, BTN_BACK } button_t;
typedef enum { BEV_NONE = 0, BEV_PRESS, BEV_REPEAT, BEV_LONG } button_event_kind_t;
typedef struct { button_event_kind_t kind; button_t button; } button_event_t;

#define BTN_DEBOUNCE_SAMPLES 2
#define BTN_REPEAT_DELAY_MS  500u
#define BTN_REPEAT_PERIOD_MS 150u
#define BTN_LONG_MS          1000u

typedef struct {
    button_t raw_last;      // last raw reading
    uint8_t  raw_count;     // consecutive identical raw readings
    button_t stable;        // debounced state
    uint32_t press_ms;      // when `stable` became non-NONE
    uint32_t next_repeat_ms;
    uint8_t  long_fired;
} buttons_t;

button_t       buttons_from_mv(uint16_t mv);
void           buttons_init(buttons_t *b);
button_event_t buttons_tick(buttons_t *b, uint16_t mv, uint32_t now_ms);
button_t       buttons_held(const buttons_t *b);
uint32_t       buttons_held_ms(const buttons_t *b, uint32_t now_ms);
#endif
