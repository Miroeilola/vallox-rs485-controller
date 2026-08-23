// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The client against the simulated machine, over the host memory bus, in real
// frames. The client side sees only panel_hal.h; the machine side only the
// membus — the same shape as every later host test.
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"
#include "vlx_client.h"

static vlx_machine_t s_m;
static vlx_client_t s_c;

// One 20 ms tick of the whole system: machine hears the panel, advances, answers; client ticks.
static void pump(void)
{
    uint8_t buf[64];
    size_t n = membus_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    hal_host_advance_ms(20);
    n = vlx_machine_tick(&s_m, hal_time_ms(), buf, sizeof buf);
    if (n) membus_machine_write(buf, n);
    vlx_client_tick(&s_c, hal_time_ms());
}

static void run_ms(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 20) pump(); }

static void setup(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    vlx_client_init(&s_c, VLX_ADDR_PANEL_DEFAULT);
}

static void test_polls_fill_the_shadow_within_three_seconds(void)
{
    setup();
    uint8_t v; uint32_t age;
    CHECK(!vlx_client_get(&s_c, VLX_REG_FAN_SPEED, &v, &age, hal_time_ms()));
    run_ms(3000);
    CHECK(vlx_client_get(&s_c, VLX_REG_FAN_SPEED, &v, &age, hal_time_ms()));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 3);
    CHECK(age < 3000);
    CHECK(vlx_client_get(&s_c, VLX_REG_TEMP_OUTDOOR, &v, &age, hal_time_ms()));
    CHECK_EQ(vlx_temp_table(v), 5);
    CHECK(vlx_client_get(&s_c, VLX_REG_STATUS, &v, &age, hal_time_ms()));
    CHECK(vlx_client_bus_ok(&s_c, hal_time_ms()));
    CHECK(!vlx_client_bus_fault(&s_c));
}

static void test_broadcasts_update_the_shadow_without_polling(void)
{
    setup();
    vlx_client_set_poll_list(&s_c, NULL, 0);
    run_ms(13000);                                  // first broadcast round at ~12 s
    uint8_t v; uint32_t age;
    CHECK(vlx_client_get(&s_c, VLX_REG_TEMP_SUPPLY, &v, &age, hal_time_ms()));
    CHECK(vlx_client_get(&s_c, VLX_REG_RH_HIGHEST, &v, &age, hal_time_ms()));
    CHECK(!vlx_client_get(&s_c, VLX_REG_STATUS, &v, &age, hal_time_ms()));   // not broadcast, not polled
    CHECK(vlx_client_bus_ok(&s_c, hal_time_ms()));
}

static void test_stale_after_thirty_seconds_of_silence(void)
{
    setup();
    run_ms(3000);
    CHECK(!vlx_client_is_stale(&s_c, VLX_REG_FAN_SPEED, hal_time_ms()));
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;          // machine goes quiet (broadcasts still come)
    vlx_client_set_poll_list(&s_c, NULL, 0);         // and we stop polling, to keep the shadow from refreshing
    run_ms(31000);
    uint8_t v; uint32_t age;
    CHECK(vlx_client_get(&s_c, VLX_REG_FAN_SPEED, &v, &age, hal_time_ms()));   // still returns the old value
    CHECK(age >= 30000);
    CHECK(vlx_client_is_stale(&s_c, VLX_REG_FAN_SPEED, hal_time_ms()));
    CHECK(vlx_client_is_stale(&s_c, 0xC1, hal_time_ms()));                       // never seen = stale
}

static void test_allowed_write_is_acknowledged_and_read_back(void)
{
    setup();
    run_ms(1000);
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(5)), VLX_WRITE_QUEUED);
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(6)), VLX_WRITE_BUSY);
    run_ms(200);
    CHECK_EQ(vlx_client_write_state(&s_c), VLX_WRITE_ACKED);
    CHECK_EQ(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED), vlx_fan_speed_to_raw(5));
    uint8_t v; uint32_t age;
    CHECK(vlx_client_get(&s_c, VLX_REG_FAN_SPEED, &v, &age, hal_time_ms()));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 5);
    vlx_client_write_clear(&s_c);
    CHECK_EQ(vlx_client_write_state(&s_c), VLX_WRITE_IDLE);
    run_ms(1000);                                    // polling resumes normally after the write
    CHECK(vlx_client_bus_ok(&s_c, hal_time_ms()));
}

static void test_write_outside_the_allow_list_is_refused_and_nothing_is_sent(void)
{
    setup();
    vlx_client_set_poll_list(&s_c, NULL, 0);
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_HEAT_SETPOINT, vlx_temp_to_raw(20)), VLX_WRITE_REFUSED);
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, 0x05), VLX_WRITE_REFUSED);   // 0x05 is not a speed
    run_ms(100);
    uint8_t buf[16];
    CHECK_EQ(membus_machine_read(buf, sizeof buf), 0);                          // the bus stayed silent
    CHECK_EQ(vlx_client_write_state(&s_c), VLX_WRITE_IDLE);
}

static void test_silent_machine_leads_to_bus_fault_and_recovery(void)
{
    setup();
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;
    run_ms(10 * VLX_CLIENT_POLL_PERIOD_MS + 500);   // ten unanswered polls
    CHECK(vlx_client_bus_fault(&s_c));
    CHECK(!vlx_client_bus_ok(&s_c, hal_time_ms()));
    s_m.reply_delay_ms = 0;
    run_ms(VLX_CLIENT_FAULT_RETRY_MS + 200);
    CHECK(!vlx_client_bus_fault(&s_c));
    CHECK(vlx_client_bus_ok(&s_c, hal_time_ms()));
}

static void test_write_without_acknowledge_fails_after_three_tries(void)
{
    setup();
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(2)), VLX_WRITE_QUEUED);
    run_ms(VLX_CLIENT_WRITE_TRIES * VLX_CLIENT_RESPONSE_MS + 100);
    CHECK_EQ(vlx_client_write_state(&s_c), VLX_WRITE_FAILED);
    CHECK_EQ(s_c.stats_write_tries, VLX_CLIENT_WRITE_TRIES);
}

static void test_tx_disabled_listens_but_never_sends(void)
{
    setup();
    vlx_client_set_tx_enabled(&s_c, false);
    run_ms(13000);
    uint8_t buf[16];
    CHECK_EQ(membus_machine_read(buf, sizeof buf), 0);
    uint8_t v; uint32_t age;
    CHECK(vlx_client_get(&s_c, VLX_REG_TEMP_OUTDOOR, &v, &age, hal_time_ms()));   // from the broadcast
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(2)), VLX_WRITE_REFUSED);
}

static void test_disabling_tx_drops_the_inflight_write(void)
{
    setup();
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;          // the write can never be acked
    CHECK_EQ(vlx_client_write(&s_c, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(5)), VLX_WRITE_QUEUED);
    // Disabled before the queued write ever reaches the wire (send happens only on
    // the next tick): this is the case the bug loses track of — without the fix,
    // write_queued stays true across the disabled span and the stale write is
    // replayed the instant tx is re-enabled.
    vlx_client_set_tx_enabled(&s_c, false);
    CHECK_EQ(vlx_client_write_state(&s_c), VLX_WRITE_FAILED);
    s_m.reply_delay_ms = 0;
    run_ms(1000);
    vlx_client_set_tx_enabled(&s_c, true);
    run_ms(4000);
    // the old command was NOT replayed once tx re-enabled; default speed 3 unchanged
    CHECK_EQ(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED), vlx_fan_speed_to_raw(3));
    CHECK(vlx_client_bus_ok(&s_c, hal_time_ms()));   // polling resumed
}

int main(void)
{
    test_polls_fill_the_shadow_within_three_seconds();
    test_broadcasts_update_the_shadow_without_polling();
    test_stale_after_thirty_seconds_of_silence();
    test_allowed_write_is_acknowledged_and_read_back();
    test_write_outside_the_allow_list_is_refused_and_nothing_is_sent();
    test_silent_machine_leads_to_bus_fault_and_recovery();
    test_write_without_acknowledge_fails_after_three_tries();
    test_tx_disabled_listens_but_never_sends();
    test_disabling_tx_drops_the_inflight_write();
    return REPORT();
}
