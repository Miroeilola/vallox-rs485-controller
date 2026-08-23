// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The panel's UI core: page stack, editor, dashboard, status page, timeouts,
// dimming, LEDs — driven by panel_ui_tick() at 50 Hz from whichever host owns
// time, seeing the world only through panel_hal.h. Free of ESP-IDF headers:
// the same source runs on the device, in the host tests and in the browser.
#ifndef PANEL_UI_H
#define PANEL_UI_H
#include <stdbool.h>
#include <stdint.h>
#include "pages.h"
#include "vlx_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_UI_TICK_MS        20u
#define PANEL_UI_RENDER_MS      200u
#define PANEL_UI_HOME_MS        60000u
#define PANEL_UI_DIM_MS         300000u
#define PANEL_UI_DIM_LEVEL      26u
#define PANEL_UI_RESET_HOLD_MS  3000u
#define PANEL_UI_SPLASH_MS      2000u
#define PANEL_UI_STACK_MAX      6

void panel_ui_init(void);
void panel_ui_tick(uint32_t now_ms);
void panel_ui_set_version(const char *v);

// Hooks for tests and the simulator side panel. Read-only.
vlx_client_t *panel_ui_client(void);
page_id_t     panel_ui_current_page(void);
int           panel_ui_page_depth(void);
int           panel_ui_list_selection(void);
int           panel_ui_editor_value(void);
bool          panel_ui_is_dimmed(void);
bool          panel_ui_splash_active(void);
int           panel_ui_dashboard_speed(void);

#ifdef __cplusplus
}
#endif
#endif
