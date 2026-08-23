// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Golden images: five deterministic scenarios rendered through the real UI,
// the real client and the simulated machine, compared pixel-exact with
// golden/<name>.png. UPDATE_GOLDEN=1 rewrites them; a deliberate look change
// updates the golden in the same PR so the diff is visible in review.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"
#include "panel_ui.h"
#include "png.h"

#define GOLDEN_DIR "golden/"

static vlx_machine_t s_m;

static void tick(void)
{
    uint8_t buf[64];
    size_t n = membus_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    hal_host_advance_ms(PANEL_UI_TICK_MS);
    n = vlx_machine_tick(&s_m, hal_time_ms(), buf, sizeof buf);
    if (n) membus_machine_write(buf, n);
    panel_ui_tick(hal_time_ms());
}
static void run_ms(uint32_t ms) { for (uint32_t t = 0; t < ms; t += PANEL_UI_TICK_MS) tick(); }
static void press_mv(uint16_t mv, uint32_t hold) { hal_host_set_buttons_mv(mv); run_ms(hold); hal_host_set_buttons_mv(3300); run_ms(300); }
#define OK_MV 819
#define PLUS_MV 430
#define BACK_MV 1336

static void scenario_begin(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    s_m.p.t_outdoor = -8.0f;                        // a winter evening: heater on
    vlx_machine_reg_set(&s_m, VLX_REG_BOOST_MINUTES, 25);
    panel_ui_set_version("0.2.0");
    panel_ui_init();
    run_ms(3000);                                   // every polled register seen at least once
}

static void scenario_dashboard(void) { scenario_begin(); }
static void scenario_list(void)      { scenario_begin(); press_mv(OK_MV, 60); }
static void scenario_editor(void)    { scenario_begin(); press_mv(OK_MV, 60); press_mv(OK_MV, 60); press_mv(PLUS_MV, 60); }
static void scenario_fault(void)     { scenario_begin(); vlx_machine_fault(&s_m, VLX_FAULT_SUPPLY_AIR_SENSOR); run_ms(3000); }
static void scenario_dimmed(void)    { scenario_begin(); run_ms(PANEL_UI_DIM_MS + 1000); }

typedef struct { const char *name; void (*run)(void); } scenario_t;
static const scenario_t k_scenarios[] = {
    {"dashboard", scenario_dashboard}, {"list", scenario_list}, {"editor", scenario_editor},
    {"fault", scenario_fault}, {"dimmed", scenario_dimmed},
};

static void compare_or_update(const char *name)
{
    char path[128], actual[128];
    snprintf(path, sizeof path, GOLDEN_DIR "%s.png", name);
    snprintf(actual, sizeof actual, GOLDEN_DIR "%s.actual.png", name);
    const uint16_t *fb = hal_host_framebuffer();
    if (getenv("UPDATE_GOLDEN")) {
        CHECK(png_write_rgb565(path, fb, HAL_DISPLAY_W, HAL_DISPLAY_H));
        printf("updated %s\n", path);
        return;
    }
    static uint16_t gold[HAL_DISPLAY_W * HAL_DISPLAY_H];
    if (!png_read_rgb565(path, gold, HAL_DISPLAY_W, HAL_DISPLAY_H)) {
        printf("FAIL missing or unreadable golden %s (run with UPDATE_GOLDEN=1 to create)\n", path);
        png_write_rgb565(actual, fb, HAL_DISPLAY_W, HAL_DISPLAY_H);
        CHECK(0);
        return;
    }
    int diff = 0, fx = -1, fy = -1;
    for (int i = 0; i < HAL_DISPLAY_W * HAL_DISPLAY_H; i++)
        if (gold[i] != fb[i]) { if (diff == 0) { fx = i % HAL_DISPLAY_W; fy = i / HAL_DISPLAY_W; } diff++; }
    if (diff) {
        png_write_rgb565(actual, fb, HAL_DISPLAY_W, HAL_DISPLAY_H);
        printf("FAIL %s: %d pixels differ, first at (%d,%d); wrote %s\n", name, diff, fx, fy, actual);
    }
    CHECK_EQ(diff, 0);
}

static void test_png_round_trip(void)
{
    static uint16_t a[HAL_DISPLAY_W * HAL_DISPLAY_H], b[HAL_DISPLAY_W * HAL_DISPLAY_H];
    for (int i = 0; i < HAL_DISPLAY_W * HAL_DISPLAY_H; i++) a[i] = (uint16_t)(i * 2654435761u >> 7);
    CHECK(png_write_rgb565(GOLDEN_DIR "roundtrip.actual.png", a, HAL_DISPLAY_W, HAL_DISPLAY_H));
    CHECK(png_read_rgb565(GOLDEN_DIR "roundtrip.actual.png", b, HAL_DISPLAY_W, HAL_DISPLAY_H));
    CHECK(memcmp(a, b, sizeof a) == 0);
    CHECK(!png_read_rgb565(GOLDEN_DIR "does-not-exist.png", b, HAL_DISPLAY_W, HAL_DISPLAY_H));
    remove(GOLDEN_DIR "roundtrip.actual.png");
}

static void test_scenarios_are_deterministic(void)
{
    // the same scenario twice gives the same pixels — otherwise goldens are noise
    static uint16_t first[HAL_DISPLAY_W * HAL_DISPLAY_H];
    scenario_dashboard();
    memcpy(first, hal_host_framebuffer(), sizeof first);
    scenario_dashboard();
    CHECK(memcmp(first, hal_host_framebuffer(), sizeof first) == 0);
}

int main(void)
{
    test_png_round_trip();
    test_scenarios_are_deterministic();
    for (size_t i = 0; i < sizeof k_scenarios / sizeof k_scenarios[0]; i++) {
        k_scenarios[i].run();
        compare_or_update(k_scenarios[i].name);
    }
    return REPORT();
}
