// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "buttons.h"
#include <string.h>

button_t buttons_from_mv(uint16_t mv)
{
    if (mv < 215)  return BTN_MINUS;   // 0 mV
    if (mv < 625)  return BTN_PLUS;    // 430 mV
    if (mv < 1078) return BTN_OK;      // 819 mV
    if (mv < 2318) return BTN_BACK;    // 1336 mV
    return BTN_NONE;                   // 3300 mV
}

void buttons_init(buttons_t *b)
{
    memset(b, 0, sizeof *b);
    b->raw_last = BTN_NONE;
    b->stable = BTN_NONE;
}

button_event_t buttons_tick(buttons_t *b, uint16_t mv, uint32_t now_ms)
{
    button_event_t ev = {BEV_NONE, BTN_NONE};
    button_t raw = buttons_from_mv(mv);
    if (raw == b->raw_last) {
        if (b->raw_count < 255) b->raw_count++;
    } else {
        b->raw_last = raw;
        b->raw_count = 1;
    }
    if (b->raw_count >= BTN_DEBOUNCE_SAMPLES && raw != b->stable) {
        b->stable = raw;
        if (raw != BTN_NONE) {                      // press (or jump from another button)
            b->press_ms = now_ms;
            b->next_repeat_ms = now_ms + BTN_REPEAT_DELAY_MS;
            b->long_fired = 0;
            ev.kind = BEV_PRESS; ev.button = raw;
            return ev;
        }
        return ev;                                  // release: no event
    }
    if (b->stable != BTN_NONE && !b->long_fired) {
        if ((int32_t)(now_ms - (b->press_ms + BTN_LONG_MS)) >= 0) {
            b->long_fired = 1;
            ev.kind = BEV_LONG; ev.button = b->stable;
            return ev;
        }
        if ((int32_t)(now_ms - b->next_repeat_ms) >= 0) {
            b->next_repeat_ms += BTN_REPEAT_PERIOD_MS;
            ev.kind = BEV_REPEAT; ev.button = b->stable;
            return ev;
        }
    }
    return ev;
}

button_t buttons_held(const buttons_t *b) { return b->stable; }

uint32_t buttons_held_ms(const buttons_t *b, uint32_t now_ms)
{
    return b->stable == BTN_NONE ? 0u : now_ms - b->press_ms;
}
