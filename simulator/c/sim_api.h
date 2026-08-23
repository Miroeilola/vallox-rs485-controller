// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser simulator's C entry points: what the JavaScript side calls into
// the WebAssembly module. Plain C ABI — integers, floats and pointers into the
// module's memory — so no Embind and no C++ runtime. One simulator instance per
// module (the page has one board).
#ifndef SIM_API_H
#define SIM_API_H
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define SIM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle and time. sim_run() advances the world in PANEL_UI_TICK_MS steps;
// elapsed_ms is clamped to SIM_MAX_STEP_MS so a tab that slept does not spin.
#define SIM_MAX_STEP_MS 200u
SIM_EXPORT void     sim_init(void);
SIM_EXPORT void     sim_run(uint32_t elapsed_ms);
SIM_EXPORT uint32_t sim_time_ms(void);
SIM_EXPORT const char *sim_version(void);

// Display: a full RGBA8888 copy of the panel, 320 x 240, row-major, top-left
// first, kept up to date by hal_display_flush. sim_fb_take_dirty() returns the
// union of the rectangles flushed since the previous call (1) or nothing (0).
SIM_EXPORT const uint8_t *sim_fb_rgba(void);
SIM_EXPORT int      sim_fb_take_dirty(int *x, int *y, int *w, int *h);
SIM_EXPORT int      sim_fb_flushes(void);       // hal_display_flush calls so far
SIM_EXPORT int      sim_backlight(void);        // 0..255
SIM_EXPORT int      sim_leds(void);             // bit0 PWR (D4), bit1 BUS (D6), bit2 FAULT (D5)

// Buttons: idx 0..3 = SW1..SW4 (− + OK ←), down 0/1. The ladder voltage the
// core sees is the rev A level of that switch (see spec §3.1).
SIM_EXPORT void     sim_button(int idx, int down);
SIM_EXPORT int      sim_button_mv(void);

// Machine side (vallox_machine) — the side panel.
SIM_EXPORT void     sim_machine_set_outdoor(float celsius);
SIM_EXPORT void     sim_machine_set_time_scale(float scale);
SIM_EXPORT void     sim_machine_set_reply_delay(int ms);    // 0, 10, 200; -1 = never
SIM_EXPORT void     sim_machine_fault(int code);
SIM_EXPORT void     sim_machine_fault_clear(void);
SIM_EXPORT float    sim_machine_temp(int which);            // 0 outdoor, 1 supply, 2 extract, 3 exhaust (model state, °C)
SIM_EXPORT int      sim_machine_reg(int reg);               // raw byte, or -1 when the model does not know the register
SIM_EXPORT int      sim_machine_fan_speed(void);            // 1..8
SIM_EXPORT int      sim_machine_flags(void);                // bit0 heating, bit1 summer bypass, bit2 supply fan stopped (frost), bit3 fault

// UI hooks for the side panel and the smoke test.
SIM_EXPORT int      sim_ui_page(void);
SIM_EXPORT int      sim_ui_depth(void);
SIM_EXPORT int      sim_ui_dimmed(void);
SIM_EXPORT int      sim_ui_bus_ok(void);
SIM_EXPORT int      sim_ui_lang(void);
SIM_EXPORT void     sim_ui_set_lang(int lang);              // 0 en, 1 fi; stored like the menu does it

// Bus log: a ring of the last SIM_LOG_MAX frames in either direction.
// Each entry is SIM_LOG_ENTRY_BYTES: u32 LE time_ms, u8 dir (0 panel→machine,
// 1 machine→panel), 6 raw bytes, 1 pad. sim_log_total() counts every frame
// ever logged; entries older than SIM_LOG_MAX are gone.
#define SIM_LOG_MAX          256
#define SIM_LOG_ENTRY_BYTES  12
SIM_EXPORT int      sim_log_total(void);
SIM_EXPORT const uint8_t *sim_log_entry(int seq);           // NULL when seq is out of the ring
SIM_EXPORT const char *sim_reg_name(int reg);
SIM_EXPORT const char *sim_fault_name(int code);

// Settings store: the RAM table behind hal_store_*, mirrored to localStorage
// by JS. Keys are NUL-terminated ASCII; values opaque bytes.
#define SIM_STORE_MAX 16
SIM_EXPORT int      sim_store_count(void);
SIM_EXPORT const char *sim_store_key(int i);
SIM_EXPORT const uint8_t *sim_store_value(int i, int *len);
SIM_EXPORT int      sim_store_put(const char *key, const uint8_t *val, int len);
SIM_EXPORT int      sim_store_take_dirty(void);             // 1 once after any put

#ifdef __cplusplus
}
#endif
#endif
