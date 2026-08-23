// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"

static void test_bus_roundtrip_panel_to_machine(void)
{
    hal_host_reset();
    const uint8_t tx[6] = {1, 2, 3, 4, 5, 6};
    CHECK_EQ(hal_bus_write(tx, 6), 6);
    uint8_t rx[16];
    CHECK_EQ(membus_machine_read(rx, sizeof rx), 6);
    CHECK(memcmp(rx, tx, 6) == 0);
    CHECK_EQ(membus_machine_read(rx, sizeof rx), 0);   // drained
}

static void test_bus_roundtrip_machine_to_panel(void)
{
    hal_host_reset();
    const uint8_t tx[3] = {0xAA, 0xBB, 0xCC};
    CHECK_EQ(membus_machine_write(tx, 3), 3);
    uint8_t rx[2];
    CHECK_EQ(hal_bus_read(rx, 2), 2);           // partial reads keep the rest
    CHECK(rx[0] == 0xAA && rx[1] == 0xBB);
    CHECK_EQ(hal_bus_read(rx, 2), 1);
    CHECK(rx[0] == 0xCC);
    CHECK_EQ(hal_bus_read(rx, 2), 0);
}

static void test_bus_ring_wraps(void)
{
    hal_host_reset();
    uint8_t b = 0;
    for (int i = 0; i < 1000; i++) {          // more than the ring size
        b = (uint8_t)i;
        CHECK_EQ(hal_bus_write(&b, 1), 1);
        uint8_t r = 0;
        CHECK_EQ(membus_machine_read(&r, 1), 1);
        CHECK_EQ(r, b);
    }
}

static void test_time_is_fake_and_monotonic(void)
{
    hal_host_reset();
    CHECK_EQ(hal_time_ms(), 0);
    hal_host_advance_ms(10);
    hal_host_advance_ms(5);
    CHECK_EQ(hal_time_ms(), 15);
}

static void test_store_roundtrip_and_length_check(void)
{
    hal_host_reset();
    uint8_t v = 7, out = 0;
    CHECK(!hal_store_get("lang", &out, 1));
    CHECK(hal_store_put("lang", &v, 1));
    CHECK(hal_store_get("lang", &out, 1));
    CHECK_EQ(out, 7);
    uint16_t wrong;
    CHECK(!hal_store_get("lang", &wrong, 2));   // length mismatch is a miss
}

static void test_display_leds_backlight_are_observable(void)
{
    hal_host_reset();
    uint16_t px[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    hal_display_flush(10, 20, 2, 2, px);
    const uint16_t *fb = hal_host_framebuffer();
    CHECK_EQ(fb[20 * HAL_DISPLAY_W + 10], 0xF800);
    CHECK_EQ(fb[20 * HAL_DISPLAY_W + 11], 0x07E0);
    CHECK_EQ(fb[21 * HAL_DISPLAY_W + 10], 0x001F);
    CHECK_EQ(fb[21 * HAL_DISPLAY_W + 11], 0xFFFF);
    hal_leds_set(true, false, true);
    bool p, b, f;
    hal_host_leds(&p, &b, &f);
    CHECK(p && !b && f);
    hal_backlight_set(200);
    CHECK_EQ(hal_host_backlight(), 200);
    hal_host_set_buttons_mv(430);
    CHECK_EQ(hal_buttons_read_mv(), 430);
}

int main(void)
{
    test_bus_roundtrip_panel_to_machine();
    test_bus_roundtrip_machine_to_panel();
    test_bus_ring_wraps();
    test_time_is_fake_and_monotonic();
    test_store_roundtrip_and_length_check();
    test_display_leds_backlight_are_observable();
    return REPORT();
}
