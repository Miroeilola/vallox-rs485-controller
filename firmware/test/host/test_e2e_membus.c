// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// A panel stub and the machine emulator, talking in real frames through the
// host memory bus. This is the shape of every later host test: the panel side
// only sees panel_hal.h, the machine side only sees the membus.
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#define PANEL VLX_ADDR_PANEL_DEFAULT
#define MACHINE VLX_ADDR_MAINBOARD_1

static vlx_machine_t s_m;

// One scheduler step: move panel->machine bytes, tick the machine, move machine->panel bytes.
static void pump(uint32_t step_ms)
{
    uint8_t buf[64];
    size_t n = membus_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    hal_host_advance_ms(step_ms);
    n = vlx_machine_tick(&s_m, hal_time_ms(), buf, sizeof buf);
    if (n) membus_machine_write(buf, n);
}

// Capture struct + callback for panel_read: the parser API takes a callback,
// so the capture is file scope (Apple clang rejects GCC nested functions
// under -std=c11 -Werror).
struct read_cap {
    vlx_frame_t fr;
    bool ok;
};

static void on_read_frame(const vlx_frame_t *fr, void *ctx)
{
    struct read_cap *cc = ctx;
    cc->fr = *fr;
    cc->ok = true;
}

// Panel stub: poll a register, wait up to deadline_ms for the answer frame.
static bool panel_read(uint8_t reg, uint8_t *value, uint32_t deadline_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_poll(PANEL, MACHINE, reg, f);
    hal_bus_write(f, VLX_FRAME_LEN);
    vlx_parser_t p;
    bool have = false;
    struct read_cap c;
    memset(&c, 0, sizeof c);
    vlx_parser_init(&p, on_read_frame, &c);
    uint32_t t0 = hal_time_ms();
    while (hal_time_ms() - t0 < deadline_ms) {
        pump(1);
        uint8_t b[16];
        size_t n = hal_bus_read(b, sizeof b);
        if (n) vlx_parser_feed_buffer(&p, b, n);
        if (c.ok && c.fr.sender == MACHINE && c.fr.receiver == PANEL && c.fr.reg == reg) {
            have = true;
            break;
        }
    }
    if (have) *value = c.fr.value;
    return have;
}

// Panel stub: write, wait for the one-byte acknowledge.
static bool panel_write(uint8_t reg, uint8_t value, uint32_t deadline_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_write(PANEL, MACHINE, reg, value, f);
    hal_bus_write(f, VLX_FRAME_LEN);
    uint32_t t0 = hal_time_ms();
    while (hal_time_ms() - t0 < deadline_ms) {
        pump(1);
        uint8_t b;
        if (hal_bus_read(&b, 1) == 1) return b == f[5];
    }
    return false;
}

static void test_panel_reads_fan_speed_through_the_bus(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    uint8_t v = 0;
    CHECK(panel_read(VLX_REG_FAN_SPEED, &v, 50));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 3);
}

static void test_panel_sets_speed_and_reads_it_back(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    // 0x29 is read-only on the real machine (speed is set through 0xA9 default / status); the
    // machine table marks 0x29 read-only, so set it through the writable default-speed register
    CHECK(panel_write(VLX_REG_FAN_SPEED_DEFAULT, vlx_fan_speed_to_raw(5), 50));
    uint8_t v = 0;
    CHECK(panel_read(VLX_REG_FAN_SPEED_DEFAULT, &v, 50));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 5);
}

static void test_panel_times_out_when_machine_never_answers(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;
    uint8_t v = 0;
    CHECK(!panel_read(VLX_REG_FAN_SPEED, &v, 10));
}

struct broadcast_cap {
    int n;
};

static void on_broadcast_frame(const vlx_frame_t *fr, void *ctx)
{
    struct broadcast_cap *cc = ctx;
    if (fr->receiver == VLX_ADDR_PANELS) cc->n++;
}

static void test_panel_sees_broadcasts_without_asking(void)
{
    hal_host_reset();
    vlx_machine_init(&s_m);
    vlx_parser_t p;
    struct broadcast_cap c;
    memset(&c, 0, sizeof c);
    vlx_parser_init(&p, on_broadcast_frame, &c);
    for (int i = 0; i < 13000; i++) {
        pump(1);
        uint8_t b[16];
        size_t n = hal_bus_read(b, sizeof b);
        if (n) vlx_parser_feed_buffer(&p, b, n);
    }
    CHECK_EQ(c.n, 7);
}

int main(void)
{
    test_panel_reads_fan_speed_through_the_bus();
    test_panel_sets_speed_and_reads_it_back();
    test_panel_times_out_when_machine_never_answers();
    test_panel_sees_broadcasts_without_asking();
    return REPORT();
}
