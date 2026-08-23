// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The machine model is exactly as right as docs/research/protocol.md. These
// tests pin the model to that document, not to a real machine.
#include <string.h>
#include "check.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#define PANEL VLX_ADDR_PANEL_DEFAULT
#define MACHINE VLX_ADDR_MAINBOARD_1

// Helpers: send one frame, tick once at now_ms, decode whatever came back.
static size_t send_and_tick(vlx_machine_t *m, const uint8_t *frame, uint32_t now_ms,
                            uint8_t *out, size_t max)
{
    vlx_machine_feed(m, frame, VLX_FRAME_LEN);
    return vlx_machine_tick(m, now_ms, out, max);
}

static void test_poll_known_register_is_answered_with_its_value(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    vlx_machine_reg_set(&m, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(3));
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_FAN_SPEED, poll);
    size_t n = send_and_tick(&m, poll, 0, out, sizeof out);
    CHECK_EQ(n, VLX_FRAME_LEN);
    vlx_frame_t f;
    CHECK(vlx_frame_decode(out, &f));
    CHECK_EQ(f.sender, MACHINE);
    CHECK_EQ(f.receiver, PANEL);
    CHECK_EQ(f.reg, VLX_REG_FAN_SPEED);
    CHECK_EQ(f.value, vlx_fan_speed_to_raw(3));
    CHECK(!vlx_frame_is_poll(&f));
}

static void test_poll_unknown_register_gets_no_answer(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    CHECK(!vlx_machine_reg_known(&m, 0xC1));     // not in the documents
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, 0xC1, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), 0);
}

static void test_poll_for_another_address_is_ignored(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, 0x22 /* another panel */, VLX_REG_FAN_SPEED, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), 0);
}

static void test_poll_to_mainboard_broadcast_is_answered(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, VLX_ADDR_MAINBOARDS, VLX_REG_STATUS, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), VLX_FRAME_LEN);
}

static void test_feed_survives_garbage_and_chunking(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    const uint8_t junk[3] = {0x55, 0xAA, 0x01};
    vlx_machine_feed(&m, junk, 3);
    vlx_machine_feed(&m, poll, 2);
    vlx_machine_feed(&m, poll + 2, 4);
    CHECK_EQ(vlx_machine_tick(&m, 0, out, sizeof out), VLX_FRAME_LEN);
}

static void test_defaults_are_a_plausible_machine(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    CHECK(vlx_machine_reg_known(&m, VLX_REG_FAN_SPEED));
    CHECK_EQ(vlx_fan_speed_from_raw(vlx_machine_reg_get(&m, VLX_REG_FAN_SPEED)), 3);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR)), 5);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT)), 18);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_CO2_SENSORS_FITTED), 0);
}

static void test_write_updates_register_and_is_acknowledged_with_checksum(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_HEAT_SETPOINT, vlx_temp_to_raw(20), w);
    size_t n = send_and_tick(&m, w, 0, out, sizeof out);
    CHECK_EQ(n, 1);
    CHECK_EQ(out[0], w[5]);                    // the acknowledge is the received checksum byte
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT), vlx_temp_to_raw(20));
}

static void test_write_to_read_only_register_is_ignored_silently(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t before = vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_TEMP_OUTDOOR, 0x80, w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR), before);
}

static void test_write_to_unknown_register_is_ignored_silently(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, 0xC1, 0x01, w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 0);
    CHECK(!vlx_machine_reg_known(&m, 0xC1));
}

static void test_write_then_poll_reads_back(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], p[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_FAN_SPEED_DEFAULT, vlx_fan_speed_to_raw(5), w);
    send_and_tick(&m, w, 0, out, sizeof out);
    vlx_make_poll(PANEL, MACHINE, VLX_REG_FAN_SPEED_DEFAULT, p);
    CHECK_EQ(send_and_tick(&m, p, 1, out, sizeof out), VLX_FRAME_LEN);
    vlx_frame_t f;
    CHECK(vlx_frame_decode(out, &f));
    CHECK_EQ(vlx_fan_speed_from_raw(f.value), 5);
}

static void test_reply_never_also_suppresses_the_acknowledge(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.reply_delay_ms = VLX_MACHINE_NEVER;
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_HEAT_SETPOINT, vlx_temp_to_raw(20), w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT), vlx_temp_to_raw(20));
}

// the output of a round is back-to-back frames; decode each six bytes
static int count_frames_to_panels(const uint8_t *buf, size_t n, uint8_t *regs_out, int max)
{
    int k = 0;
    for (size_t i = 0; i + VLX_FRAME_LEN <= n; i += VLX_FRAME_LEN) {
        vlx_frame_t f;
        if (vlx_frame_decode(buf + i, &f) && f.receiver == VLX_ADDR_PANELS && k < max) regs_out[k++] = f.reg;
    }
    return k;
}

static void test_reply_delay_holds_the_answer_until_due(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.reply_delay_ms = 10;
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    vlx_machine_feed(&m, poll, VLX_FRAME_LEN);
    CHECK_EQ(vlx_machine_tick(&m, 100, out, sizeof out), 0);    // queued at 100, due 110
    CHECK_EQ(vlx_machine_tick(&m, 105, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_tick(&m, 110, out, sizeof out), VLX_FRAME_LEN);
}

static void test_reply_never_means_silence(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.reply_delay_ms = VLX_MACHINE_NEVER;
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    vlx_machine_feed(&m, poll, VLX_FRAME_LEN);
    CHECK_EQ(vlx_machine_tick(&m, 0, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_tick(&m, 5000, out, sizeof out), 0);
}

static void test_broadcast_round_every_12_s_in_documented_order(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t out[64], regs[16];
    size_t total = 0; uint8_t all[256];
    // walk 13 s in 10 ms ticks, collect everything sent
    for (uint32_t t = 0; t <= 13000; t += 10) {
        size_t n = vlx_machine_tick(&m, t, out, sizeof out);
        if (n && total + n <= sizeof all) { memcpy(all + total, out, n); total += n; }
    }
    int k = count_frames_to_panels(all, total, regs, 16);
    CHECK_EQ(k, 7);
    const uint8_t expect[7] = {0x2B, 0x2C, 0x35, 0x34, 0x32, 0x33, 0x2A};
    CHECK(k == 7 && memcmp(regs, expect, 7) == 0);
}

static void test_broadcast_frames_are_spaced_130_ms(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t out[64];
    uint32_t first = 0, second = 0;
    for (uint32_t t = 0; t <= 13000; t += 10) {
        if (vlx_machine_tick(&m, t, out, sizeof out)) {
            if (!first) first = t; else if (!second) second = t;
        }
    }
    CHECK_EQ(first, 12000);
    CHECK_EQ(second - first, 130);
}

static void run_physics(vlx_machine_t *m, float seconds, float dt)
{
    for (float t = 0; t < seconds; t += dt) vlx_machine_physics_step(m, dt);
}

static void test_cold_outdoor_turns_heater_on_and_supply_reaches_setpoint(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -20.0f;
    run_physics(&m, 3600.0f, 1.0f);
    int16_t supply = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_SUPPLY));
    int16_t sp = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT));
    CHECK(supply >= sp - 1 && supply <= sp + 1);
    CHECK(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_HEATING);   // name from vallox_protocol.h
}

static void test_mild_outdoor_keeps_heater_off_and_supply_below_setpoint(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = 15.0f;
    run_physics(&m, 3600.0f, 1.0f);
    CHECK(!(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_HEATING));
    int16_t supply = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_SUPPLY));
    // recovered = 15 + 0.6*(21-15) = 18.6 ≈ setpoint 18 → heater off, supply ~18-19
    CHECK(supply >= 18 && supply <= 19);
}

static void test_higher_fan_speed_settles_faster(void)
{
    vlx_machine_t a, b;
    vlx_machine_init(&a); vlx_machine_init(&b);
    a.p.t_outdoor = b.p.t_outdoor = -20.0f;
    vlx_machine_reg_set(&a, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(1));
    vlx_machine_reg_set(&b, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(8));
    run_physics(&a, 300.0f, 1.0f);
    run_physics(&b, 300.0f, 1.0f);
    // b moved further toward its target in the same time
    CHECK(b.t_exhaust < a.t_exhaust);
}

static void test_frost_protection_stops_supply_fan_and_releases_with_hysteresis(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -30.0f;
    run_physics(&m, 7200.0f, 1.0f);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_SUPPLY_FAN_STOP), 1);
    m.p.t_outdoor = 10.0f;
    run_physics(&m, 7200.0f, 1.0f);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_SUPPLY_FAN_STOP), 0);
}

static void test_time_scale_speeds_everything_up(void)
{
    vlx_machine_t a, b;
    vlx_machine_init(&a); vlx_machine_init(&b);
    a.p.t_outdoor = b.p.t_outdoor = -20.0f;
    b.p.time_scale = 60.0f;
    run_physics(&a, 60.0f, 1.0f);
    run_physics(&b, 60.0f, 1.0f);
    CHECK(b.t_exhaust < a.t_exhaust);
}

static void test_tick_runs_physics_and_updates_registers(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -20.0f;
    m.p.time_scale = 60.0f;
    uint8_t out[64];
    uint8_t before = vlx_machine_reg_get(&m, VLX_REG_TEMP_EXHAUST);
    for (uint32_t t = 0; t <= 60000; t += 20) vlx_machine_tick(&m, t, out, sizeof out);
    CHECK(vlx_machine_reg_get(&m, VLX_REG_TEMP_EXHAUST) != before);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR)), -20);
}

int main(void)
{
    test_poll_known_register_is_answered_with_its_value();
    test_poll_unknown_register_gets_no_answer();
    test_poll_for_another_address_is_ignored();
    test_poll_to_mainboard_broadcast_is_answered();
    test_feed_survives_garbage_and_chunking();
    test_defaults_are_a_plausible_machine();
    test_write_updates_register_and_is_acknowledged_with_checksum();
    test_write_to_read_only_register_is_ignored_silently();
    test_write_to_unknown_register_is_ignored_silently();
    test_write_then_poll_reads_back();
    test_reply_never_also_suppresses_the_acknowledge();
    test_reply_delay_holds_the_answer_until_due();
    test_reply_never_means_silence();
    test_broadcast_round_every_12_s_in_documented_order();
    test_broadcast_frames_are_spaced_130_ms();
    test_cold_outdoor_turns_heater_on_and_supply_reaches_setpoint();
    test_mild_outdoor_keeps_heater_off_and_supply_below_setpoint();
    test_higher_fan_speed_settles_faster();
    test_frost_protection_stops_supply_fan_and_releases_with_hysteresis();
    test_time_scale_speeds_everything_up();
    test_tick_runs_physics_and_updates_registers();
    return REPORT();
}
