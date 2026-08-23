// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The UI core against the simulated machine over the memory bus: buttons are
// ladder voltages, time is the fake clock, the bus carries real frames.
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"
#include "vlx_client.h"
#include "panel_ui.h"
#include "pages.h"
#include "texts.h"
#include "gfx.h"

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

static uint16_t mv_of(int btn)
{
    switch (btn) { case 1: return 0; case 2: return 430; case 3: return 819; case 4: return 1336; default: return 3300; }
}
#define B_MINUS 1
#define B_PLUS 2
#define B_OK 3
#define B_BACK 4
// press: hold for `hold_ms`, release, settle 100 ms (lets the ack / redraw happen)
static void press_for(int btn, uint32_t hold_ms)
{
    hal_host_set_buttons_mv(mv_of(btn)); run_ms(hold_ms);
    hal_host_set_buttons_mv(3300);       run_ms(100);
}
static void press(int btn) { press_for(btn, 60); }

static void setup(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    panel_ui_init();
    run_ms(1500);                                   // shadow fills
}

static void test_boots_to_the_dashboard_with_power_led_and_full_backlight(void)
{
    setup();
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
    CHECK_EQ(panel_ui_page_depth(), 1);
    bool p, b, f; hal_host_leds(&p, &b, &f);
    CHECK(p); CHECK(b); CHECK(!f);
    CHECK_EQ(hal_host_backlight(), 255);
    CHECK_EQ(panel_ui_dashboard_speed(), 3);
    CHECK(!panel_ui_is_dimmed());
}

static void test_ok_opens_menu_and_back_returns(void)
{
    setup();
    press(B_OK);
    CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
    CHECK_EQ(panel_ui_page_depth(), 2);
    CHECK_EQ(panel_ui_list_selection(), 0);
    press(B_BACK);
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
}

static void test_list_navigation_and_open(void)
{
    setup();
    press(B_OK);
    press(B_PLUS);  CHECK_EQ(panel_ui_list_selection(), 1);
    press(B_PLUS);  CHECK_EQ(panel_ui_list_selection(), 2);
    press(B_PLUS);  CHECK_EQ(panel_ui_list_selection(), 3);
    press(B_PLUS);  CHECK_EQ(panel_ui_list_selection(), 3);        // clamps at the last row
    press(B_MINUS); CHECK_EQ(panel_ui_list_selection(), 2);
    press(B_OK);    CHECK_EQ(panel_ui_current_page(), PAGE_STATUS);
    press(B_BACK);  CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
    CHECK_EQ(panel_ui_list_selection(), 2);                        // selection remembered
    press(B_MINUS); press(B_MINUS); press(B_OK);
    CHECK_EQ(panel_ui_current_page(), PAGE_FAN_SPEED);
    CHECK_EQ(panel_ui_editor_value(), 3);                          // starts at the shadow value
}

static void test_editor_clamps_to_min_and_max(void)
{
    setup();
    press(B_OK); press(B_OK);                                      // menu → fan speed editor
    for (int i = 0; i < 10; i++) press(B_PLUS);
    CHECK_EQ(panel_ui_editor_value(), 8);
    for (int i = 0; i < 10; i++) press(B_MINUS);
    CHECK_EQ(panel_ui_editor_value(), 1);
}

static void test_editor_repeat_counts(void)
{
    setup();
    press(B_OK); press(B_OK);
    // hold + for 1 s: PRESS + repeats at 500/650/800/950 → 3 + 5 = 8
    press_for(B_PLUS, 1000);
    CHECK_EQ(panel_ui_editor_value(), 8);
}

static void test_editor_save_writes_the_register_and_returns(void)
{
    setup();
    vlx_machine_reg_set(&s_m, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(1));
    run_ms(3000);                                                  // shadow catches up
    press(B_OK); press(B_OK);
    CHECK_EQ(panel_ui_editor_value(), 1);
    press(B_PLUS); press(B_PLUS); press(B_PLUS);
    press(B_OK);                                                   // save
    run_ms(300);
    CHECK_EQ(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED), 0x0F);
    CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
}

static void test_back_leaves_editor_without_writing(void)
{
    setup();
    press(B_OK); press(B_OK);
    press(B_PLUS); press(B_PLUS);
    press(B_BACK);
    run_ms(300);
    CHECK_EQ(vlx_fan_speed_from_raw(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED)), 3);
    CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
}

static void test_e2e_plus_three_times_on_the_dashboard_sets_speed_4(void)
{
    // spec §4: "press + three times → speed 4 on the dashboard and register 0x29 reads 0x0F"
    setup();
    vlx_machine_reg_set(&s_m, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(1));
    run_ms(3000);
    CHECK_EQ(panel_ui_dashboard_speed(), 1);
    press(B_PLUS); run_ms(300);
    press(B_PLUS); run_ms(300);
    press(B_PLUS); run_ms(300);
    CHECK_EQ(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED), 0x0F);
    CHECK_EQ(panel_ui_dashboard_speed(), 4);
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
}

static void test_read_only_editor_does_not_change_or_write(void)
{
    setup();
    uint8_t before = vlx_machine_reg_get(&s_m, VLX_REG_HEAT_SETPOINT);
    press(B_OK); press(B_PLUS); press(B_OK);                       // menu → heating setpoint
    CHECK_EQ(panel_ui_current_page(), PAGE_HEAT_SETPOINT);
    CHECK_EQ(panel_ui_editor_value(), 18);
    press(B_PLUS); press(B_PLUS);
    CHECK_EQ(panel_ui_editor_value(), 18);                         // inert: not on the allow-list
    press(B_OK);
    run_ms(300);
    CHECK_EQ(vlx_machine_reg_get(&s_m, VLX_REG_HEAT_SETPOINT), before);
    CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
}

static void test_language_setting_is_applied_stored_and_restored(void)
{
    setup();
    CHECK_EQ(text_lang(), LANG_EN);
    press(B_OK); press(B_PLUS); press(B_PLUS); press(B_PLUS); press(B_OK);   // settings
    CHECK_EQ(panel_ui_current_page(), PAGE_SETTINGS);
    press(B_OK);                                                            // language
    CHECK_EQ(panel_ui_current_page(), PAGE_LANGUAGE);
    press(B_PLUS); press(B_OK);
    CHECK_EQ(text_lang(), LANG_FI);
    uint8_t stored = 9;
    CHECK(hal_store_get("lang", &stored, 1)); CHECK_EQ(stored, 1);
    panel_ui_init();                                                        // reboot keeps the store
    CHECK_EQ(text_lang(), LANG_FI);
}

static void test_returns_to_dashboard_after_60_s_idle(void)
{
    setup();
    press(B_OK); press(B_OK);
    CHECK_EQ(panel_ui_current_page(), PAGE_FAN_SPEED);
    run_ms(PANEL_UI_HOME_MS + 500);
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
}

static void test_dims_after_5_min_and_a_press_wakes_without_acting(void)
{
    setup();
    run_ms(PANEL_UI_DIM_MS + 500);
    CHECK(panel_ui_is_dimmed());
    CHECK_EQ(hal_host_backlight(), PANEL_UI_DIM_LEVEL);
    press(B_OK);
    CHECK(!panel_ui_is_dimmed());
    CHECK_EQ(hal_host_backlight(), 255);
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);            // the waking press was consumed
    press(B_OK);
    CHECK_EQ(panel_ui_current_page(), PAGE_MENU);
}

static void test_long_back_goes_home_from_anywhere(void)
{
    setup();
    press(B_OK); press(B_PLUS); press(B_PLUS); press(B_PLUS); press(B_OK); press(B_OK);   // language editor
    CHECK_EQ(panel_ui_current_page(), PAGE_LANGUAGE);
    press_for(B_BACK, 1200);
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
}

static void test_factory_reset_when_back_is_held_at_power_up(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    uint8_t fi = 1; hal_store_put("lang", &fi, 1);
    hal_host_set_buttons_mv(mv_of(B_BACK));                        // held before power-up
    panel_ui_init();
    CHECK_EQ(text_lang(), LANG_FI);
    run_ms(PANEL_UI_RESET_HOLD_MS + 200);
    hal_host_set_buttons_mv(3300);
    CHECK(panel_ui_splash_active());
    CHECK_EQ(text_lang(), LANG_EN);
    uint8_t stored = 9; CHECK(hal_store_get("lang", &stored, 1)); CHECK_EQ(stored, 0);
    run_ms(PANEL_UI_SPLASH_MS + 200);
    CHECK(!panel_ui_splash_active());
    CHECK_EQ(panel_ui_current_page(), PAGE_DASHBOARD);
}

static void test_leds_follow_fault_and_bus(void)
{
    setup();
    vlx_machine_fault(&s_m, VLX_FAULT_SUPPLY_AIR_SENSOR);
    run_ms(3000);
    bool p, b, f; hal_host_leds(&p, &b, &f);
    CHECK(f);
    vlx_machine_fault_clear(&s_m);
    run_ms(3000);
    hal_host_leds(&p, &b, &f);
    CHECK(!f);
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;
    run_ms(4000);
    hal_host_leds(&p, &b, &f);
    CHECK(!b);
    CHECK(panel_ui_client()->bus_fault);
}

int main(void)
{
    test_boots_to_the_dashboard_with_power_led_and_full_backlight();
    test_ok_opens_menu_and_back_returns();
    test_list_navigation_and_open();
    test_editor_clamps_to_min_and_max();
    test_editor_repeat_counts();
    test_editor_save_writes_the_register_and_returns();
    test_back_leaves_editor_without_writing();
    test_e2e_plus_three_times_on_the_dashboard_sets_speed_4();
    test_read_only_editor_does_not_change_or_write();
    test_language_setting_is_applied_stored_and_restored();
    test_returns_to_dashboard_after_60_s_idle();
    test_dims_after_5_min_and_a_press_wakes_without_acting();
    test_long_back_goes_home_from_anywhere();
    test_factory_reset_when_back_is_held_at_power_up();
    test_leds_follow_fault_and_bus();
    return REPORT();
}
