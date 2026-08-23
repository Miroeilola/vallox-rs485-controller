// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser HAL and the simulator entry points, compiled natively: the same
// C the WebAssembly module runs, checked without a browser. The e2e over the
// memory bus ("+ three times → register 0x29") is repeated here through the
// sim_* API so the exported surface JS relies on is the thing under test.
#include <string.h>
#include "check.h"
#include "hal_web.h"
#include "panel_hal.h"
#include "sim_api.h"
#include "vallox_protocol.h"

static void test_ladder_levels_and_lowest_pressed_wins(void)
{
    hal_web_reset();
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);
    hal_web_set_button(2, true);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW3_MV);
    hal_web_set_button(0, true);                 // SW1 (0 mV) wins over SW3
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW1_MV);
    hal_web_set_button(9, true);                 // out of range: ignored
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW1_MV);
}

static void test_release_waits_for_min_hold_samples(void)
{
    hal_web_reset();
    hal_web_set_button(1, true);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 1
    hal_web_set_button(1, false);                              // released too early
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 2: still down
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 3: still down
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);  // released after 3 samples
    // a long hold releases at once
    hal_web_set_button(1, true);
    for (int i = 0; i < 10; i++) hal_buttons_read_mv();
    hal_web_set_button(1, false);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);
    // re-press during a pending release restarts the hold
    hal_web_set_button(3, true); hal_buttons_read_mv();
    hal_web_set_button(3, false); hal_web_set_button(3, true);
    for (int i = 0; i < 5; i++) CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW4_MV);
}

static void test_flush_converts_rgb565_and_unions_dirty_rects(void)
{
    hal_web_reset();
    int x, y, w, h;
    CHECK(!hal_web_take_dirty(&x, &y, &w, &h));
    uint16_t px[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };   // red, green, blue, white
    hal_display_flush(10, 20, 2, 2, px);
    const uint8_t *fb = hal_web_rgba();
    const uint8_t *p = &fb[(20 * HAL_DISPLAY_W + 10) * 4];
    CHECK(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255);
    CHECK(p[4] == 0 && p[5] == 255 && p[6] == 0);
    p = &fb[(21 * HAL_DISPLAY_W + 10) * 4];
    CHECK(p[0] == 0 && p[1] == 0 && p[2] == 255);
    CHECK(p[4] == 255 && p[5] == 255 && p[6] == 255);
    uint16_t one = 0x0000;
    hal_display_flush(300, 230, 1, 1, &one);
    CHECK(hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(x, 10); CHECK_EQ(y, 20); CHECK_EQ(w, 291); CHECK_EQ(h, 211);
    CHECK(!hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(hal_web_flushes(), 2);
    // clipped at the edge: no write past the buffer, union clamped
    uint16_t row[8]; memset(row, 0xFF, sizeof row);
    hal_display_flush(316, 238, 8, 8, row);
    CHECK(hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(x, 316); CHECK_EQ(y, 238); CHECK_EQ(w, 4); CHECK_EQ(h, 2);
}

static void test_leds_backlight_and_store(void)
{
    hal_web_reset();
    hal_leds_set(true, false, true);
    CHECK_EQ(hal_web_leds(), 1 | 4);
    hal_backlight_set(26);
    CHECK_EQ(hal_web_backlight(), 26);
    uint8_t one = 1, got = 9;
    CHECK(hal_web_store_take_dirty() == false);
    CHECK(hal_store_put("lang", &one, 1));
    CHECK(hal_web_store_take_dirty());
    CHECK(!hal_web_store_take_dirty());
    CHECK(hal_store_get("lang", &got, 1)); CHECK_EQ(got, 1);
    CHECK(!hal_store_get("lang", &got, 2));           // length mismatch
    CHECK_EQ(hal_web_store_count(), 1);
    CHECK(strcmp(hal_web_store_key(0), "lang") == 0);
    int len = 0; const uint8_t *v = hal_web_store_value(0, &len);
    CHECK(v && len == 1 && v[0] == 1);
    CHECK(hal_web_store_key(1) == NULL);
    hal_web_reset();                                   // the store survives a reset (it is the NVS)
    CHECK(hal_store_get("lang", &got, 1));
}

static void test_bus_log_records_frames_in_both_directions(void)
{
    hal_web_reset();
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_poll(VLX_ADDR_PANEL_DEFAULT, VLX_ADDR_MAINBOARD_1, VLX_REG_FAN_SPEED, f);
    hal_web_set_time_ms(1234);
    CHECK_EQ(hal_bus_write(f, 3), 3);                  // half a frame: nothing logged yet
    CHECK_EQ(hal_web_log_total(), 0);
    CHECK_EQ(hal_bus_write(f + 3, 3), 3);
    CHECK_EQ(hal_web_log_total(), 1);
    const uint8_t *e = hal_web_log_entry(0);
    CHECK(e != NULL);
    CHECK_EQ(e[0] | (e[1] << 8), 1234); CHECK_EQ(e[4], 0);
    CHECK(memcmp(e + 5, f, VLX_FRAME_LEN) == 0);
    // the machine's answer, logged as direction 1 and readable by the panel
    vlx_make_write(VLX_ADDR_MAINBOARD_1, VLX_ADDR_PANEL_DEFAULT, VLX_REG_FAN_SPEED, 0x0F, f);
    hal_web_machine_write(f, VLX_FRAME_LEN);
    CHECK_EQ(hal_web_log_total(), 2);
    e = hal_web_log_entry(1);
    CHECK(e && e[4] == 1 && e[8] == VLX_REG_FAN_SPEED && e[9] == 0x0F);
    uint8_t rx[8]; CHECK_EQ(hal_bus_read(rx, sizeof rx), VLX_FRAME_LEN);
    // the panel's bytes reach the machine's end
    uint8_t m[8]; CHECK_EQ(hal_web_machine_read(m, sizeof m), VLX_FRAME_LEN);
    // ring: old entries fall off
    for (int i = 0; i < HAL_WEB_LOG_MAX + 5; i++) hal_web_machine_write(f, VLX_FRAME_LEN);
    CHECK_EQ(hal_web_log_total(), 2 + HAL_WEB_LOG_MAX + 5);
    CHECK(hal_web_log_entry(0) == NULL);
    CHECK(hal_web_log_entry(7) != NULL);
    CHECK(hal_web_log_entry(hal_web_log_total()) == NULL);
}

static void run_ms(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 20) sim_run(20); }
static void press(int idx) { sim_button(idx, 1); run_ms(60); sim_button(idx, 0); run_ms(300); }

static void test_sim_plus_three_times_reaches_the_machine(void)
{
    sim_init();
    CHECK_EQ(sim_time_ms(), 0);
    run_ms(3000);
    CHECK_EQ(sim_time_ms(), 3000);
    CHECK(sim_ui_bus_ok());
    CHECK(sim_fb_flushes() > 0);
    int x, y, w, h;
    CHECK(sim_fb_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(w, HAL_DISPLAY_W); CHECK_EQ(h, HAL_DISPLAY_H);   // the first render is a full frame
    CHECK_EQ(sim_backlight(), 255);
    CHECK_EQ(sim_leds() & 1, 1);                               // PWR
    int before = sim_machine_fan_speed();
    CHECK(before >= 1 && before <= 8);
    press(1); press(1); press(1);
    CHECK_EQ(sim_machine_fan_speed(), before + 3);
    CHECK_EQ(sim_machine_reg(VLX_REG_FAN_SPEED), vlx_fan_speed_to_raw(before + 3));
    CHECK(sim_log_total() > 10);
    CHECK(sim_log_entry(sim_log_total() - 1) != NULL);
    CHECK_EQ(sim_machine_reg(0xFE), -1);                       // not a register the model knows
    CHECK(strcmp(sim_reg_name(VLX_REG_FAN_SPEED), "") != 0);
    CHECK(strcmp(sim_fault_name(VLX_FAULT_SUPPLY_AIR_SENSOR), "") != 0);
}

static void test_sim_run_is_clamped_and_carries_the_remainder(void)
{
    sim_init();
    sim_run(5000);
    CHECK_EQ(sim_time_ms(), SIM_MAX_STEP_MS);
    sim_run(7); sim_run(7); sim_run(7);        // 21 ms → one tick, 1 ms carried
    CHECK_EQ(sim_time_ms(), SIM_MAX_STEP_MS + 20);
}

static void test_sim_machine_controls_and_fault(void)
{
    sim_init();
    sim_machine_set_outdoor(-20.0f);
    CHECK(sim_machine_temp(0) < -19.9f && sim_machine_temp(0) > -20.1f);
    sim_machine_set_time_scale(60.0f);
    run_ms(5000);
    CHECK(sim_machine_temp(1) < sim_machine_temp(2));          // supply below extract at -20 °C out
    CHECK_EQ(sim_machine_flags() & 8, 0);
    sim_machine_fault(VLX_FAULT_SUPPLY_AIR_SENSOR);
    run_ms(4000);                                              // the poll round visits 0x36 within 2.75 s
    CHECK_EQ(sim_machine_flags() & 8, 8);
    CHECK_EQ(sim_leds() & 4, 4);                               // FAULT LED follows
    sim_machine_fault_clear();
    run_ms(4000);
    CHECK_EQ(sim_machine_flags() & 8, 0);
    sim_machine_set_reply_delay(-1);                           // never answer
    run_ms(6000);
    CHECK(!sim_ui_bus_ok());
    sim_machine_set_reply_delay(0);
    run_ms(3000);
    CHECK(sim_ui_bus_ok());
}

static void test_sim_language_and_store_mirror(void)
{
    // the store is the device's NVS: it survives sim_init(), so JS restores
    // localStorage into it BEFORE sim_init() and the UI reads it at init
    uint8_t zero = 0;
    CHECK(sim_store_put("lang", &zero, 1));
    sim_init();
    CHECK_EQ(sim_ui_lang(), 0);
    sim_ui_set_lang(1);
    CHECK_EQ(sim_ui_lang(), 1);
    CHECK(sim_store_take_dirty());
    CHECK_EQ(sim_store_count(), 1);
    CHECK(strcmp(sim_store_key(0), "lang") == 0);
    int len = 0; const uint8_t *v = sim_store_value(0, &len);
    CHECK(v && len == 1 && v[0] == 1);
    // a put from JS is not reported back to JS as dirty
    CHECK(sim_store_put("lang", &zero, 1));
    CHECK(!sim_store_take_dirty());
    sim_init();                                                // the UI reads the store at init
    CHECK_EQ(sim_ui_lang(), 0);
}

int main(void)
{
    test_ladder_levels_and_lowest_pressed_wins();
    test_release_waits_for_min_hold_samples();
    test_flush_converts_rgb565_and_unions_dirty_rects();
    test_leds_backlight_and_store();
    test_bus_log_records_frames_in_both_directions();
    test_sim_plus_three_times_reaches_the_machine();
    test_sim_run_is_clamped_and_carries_the_remainder();
    test_sim_machine_controls_and_fault();
    test_sim_language_and_store_mirror();
    return REPORT();
}
