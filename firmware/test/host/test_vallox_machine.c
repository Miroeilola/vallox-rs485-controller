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
    return REPORT();
}
