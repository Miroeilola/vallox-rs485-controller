// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Unit tests for the Vallox DIGIT codec.
//
// These do not prove the protocol is right — nothing has been measured on a
// machine yet. They prove the implementation matches what the research says,
// which is the part that can be checked without hardware, and they are what
// makes a wrong assumption cheap to correct later: change the table, run the
// tests, see exactly what moved.

#include <stdio.h>
#include <string.h>

#include "vallox_protocol.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

typedef struct {
    unsigned calls;
    vlx_frame_t last;
} capture_t;

static void capture_cb(const vlx_frame_t *f, void *ctx)
{
    capture_t *c = (capture_t *)ctx;
    c->calls++;
    c->last = *f;
}

// ---------------------------------------------------------------------------

// The worked example carried by the reverse-engineered documentation: panel
// 0x21 polling the mainboard for register 0xA3.
//
// The source writes the checksum as 0xC9. It is 0xD6:
// 0x01+0x21+0x11+0x00+0xA3 = 214. Two independent implementations compute the
// checksum the way this code does, so the annotated example is wrong and the
// rule is right. Recorded here rather than quietly corrected, because it is a
// concrete reminder of how much of this material is transcription.
static void test_documented_example(void)
{
    const uint8_t expected[VLX_FRAME_LEN] = {0x01, 0x21, 0x11, 0x00, 0xA3, 0xD6};
    CHECK(vlx_checksum(expected) == 0xD6);

    uint8_t out[VLX_FRAME_LEN];
    vlx_make_poll(0x21, VLX_ADDR_MAINBOARD_1, VLX_REG_STATUS, out);
    CHECK(memcmp(out, expected, sizeof(out)) == 0);

    vlx_frame_t f;
    CHECK(vlx_frame_decode(expected, &f));
    CHECK(vlx_frame_is_poll(&f));
    CHECK(f.value == VLX_REG_STATUS);
}

static void test_checksum_wraps(void)
{
    // The checksum is the low byte of the sum, so it has to wrap rather than
    // saturate. Five bytes of 0xFF sum to 0x4FB.
    const uint8_t raw[VLX_FRAME_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    CHECK(vlx_checksum(raw) == 0xFB);
}

static void test_write_is_not_a_poll(void)
{
    // Swapping the register and value fields turns a read into a write. That
    // mistake is silent on the bus and expensive on the machine.
    uint8_t poll[VLX_FRAME_LEN];
    uint8_t write[VLX_FRAME_LEN];
    vlx_make_poll(0x27, VLX_ADDR_MAINBOARD_1, VLX_REG_FAN_SPEED, poll);
    vlx_make_write(0x27, VLX_ADDR_MAINBOARD_1, VLX_REG_FAN_SPEED, 0x07, write);

    CHECK(poll[3] == VLX_POLL);
    CHECK(poll[4] == VLX_REG_FAN_SPEED);
    CHECK(write[3] == VLX_REG_FAN_SPEED);
    CHECK(write[4] == 0x07);

    vlx_frame_t f;
    CHECK(vlx_frame_decode(write, &f));
    CHECK(!vlx_frame_is_poll(&f));
}

static void test_encode_recomputes_checksum(void)
{
    vlx_frame_t f = {VLX_DOMAIN, 0x11, 0x21, VLX_REG_TEMP_SUPPLY, 0xAD, 0x00};
    uint8_t out[VLX_FRAME_LEN];
    vlx_frame_encode(&f, out);
    CHECK(f.checksum == out[5]);

    vlx_frame_t back;
    CHECK(vlx_frame_decode(out, &back));
    CHECK(back.sender == 0x11 && back.receiver == 0x21);
    CHECK(back.reg == VLX_REG_TEMP_SUPPLY && back.value == 0xAD);
}

// ---------------------------------------------------------------------------

static void test_parser_accepts_a_clean_frame(void)
{
    uint8_t frame[VLX_FRAME_LEN];
    vlx_make_write(0x11, VLX_ADDR_PANELS, VLX_REG_FAN_SPEED, 0x0F, frame);

    capture_t cap = {0};
    vlx_parser_t p;
    vlx_parser_init(&p, capture_cb, &cap);
    CHECK(vlx_parser_feed_buffer(&p, frame, sizeof(frame)) == 1);
    CHECK(cap.calls == 1);
    CHECK(cap.last.reg == VLX_REG_FAN_SPEED);
    CHECK(vlx_fan_speed_from_raw(cap.last.value) == 4);
    CHECK(p.stats.frames_ok == 1);
    CHECK(p.stats.bytes_discarded == 0);
}

static void test_parser_survives_arbitrary_chunking(void)
{
    // A UART hands over whatever happened to be in the FIFO. The parser must
    // not care where the read boundary falls.
    uint8_t frame[VLX_FRAME_LEN];
    vlx_make_write(0x11, 0x21, VLX_REG_TEMP_OUTDOOR, 0x64, frame);

    for (size_t split = 1; split < VLX_FRAME_LEN; split++) {
        capture_t cap = {0};
        vlx_parser_t p;
        vlx_parser_init(&p, capture_cb, &cap);
        vlx_parser_feed_buffer(&p, frame, split);
        vlx_parser_feed_buffer(&p, frame + split, VLX_FRAME_LEN - split);
        CHECK(cap.calls == 1);
    }
}

static void test_parser_resyncs_after_lost_bytes(void)
{
    // There is no start delimiter, so recovering alignment means sliding one
    // byte at a time until a checksum passes. This is the behaviour that
    // decides whether a capture survives a single dropped byte.
    uint8_t frame[VLX_FRAME_LEN];
    vlx_make_write(0x11, 0x21, VLX_REG_FAN_SPEED, 0x1F, frame);

    uint8_t stream[16];
    const uint8_t noise[] = {0x55, 0xAA, 0x01, 0x02};
    memcpy(stream, noise, sizeof(noise));
    memcpy(stream + sizeof(noise), frame, VLX_FRAME_LEN);

    capture_t cap = {0};
    vlx_parser_t p;
    vlx_parser_init(&p, capture_cb, &cap);
    CHECK(vlx_parser_feed_buffer(&p, stream, sizeof(noise) + VLX_FRAME_LEN) == 1);
    CHECK(cap.calls == 1);
    CHECK(cap.last.value == 0x1F);
    CHECK(p.stats.bytes_discarded == sizeof(noise));
}

static void test_parser_rejects_bad_checksum(void)
{
    uint8_t frame[VLX_FRAME_LEN];
    vlx_make_write(0x11, 0x21, VLX_REG_FAN_SPEED, 0x3F, frame);
    frame[5] ^= 0xFF;

    capture_t cap = {0};
    vlx_parser_t p;
    vlx_parser_init(&p, capture_cb, &cap);
    CHECK(vlx_parser_feed_buffer(&p, frame, sizeof(frame)) == 0);
    CHECK(cap.calls == 0);
    CHECK(p.stats.checksum_rejects == 1);
}

static void test_parser_counts_domain_rejects_separately(void)
{
    // A wrong domain byte and a wrong checksum are different faults. One means
    // the alignment is off, the other means the line is noisy, and a field
    // diagnostic that cannot tell them apart is not worth logging.
    const uint8_t junk[VLX_FRAME_LEN] = {0x02, 0x11, 0x21, 0x29, 0x07, 0x00};
    vlx_parser_t p;
    vlx_parser_init(&p, NULL, NULL);
    vlx_parser_feed_buffer(&p, junk, sizeof(junk));
    CHECK(p.stats.domain_rejects == 1);
    CHECK(p.stats.checksum_rejects == 0);
}

static void test_parser_reset_drops_partial_alignment(void)
{
    vlx_parser_t p;
    vlx_parser_init(&p, NULL, NULL);
    vlx_parser_feed(&p, 0x01);
    vlx_parser_feed(&p, 0x11);
    CHECK(p.fill == 2);
    vlx_parser_reset(&p);
    CHECK(p.fill == 0);

    // After a reset the next full frame still parses.
    uint8_t frame[VLX_FRAME_LEN];
    vlx_make_write(0x11, 0x21, VLX_REG_FAN_SPEED, 0x01, frame);
    CHECK(vlx_parser_feed_buffer(&p, frame, sizeof(frame)) == 1);
}

static void test_parser_handles_back_to_back_frames(void)
{
    uint8_t stream[VLX_FRAME_LEN * 3];
    vlx_make_write(0x11, 0x21, VLX_REG_TEMP_OUTDOOR, 0x64, stream);
    vlx_make_write(0x11, 0x21, VLX_REG_TEMP_SUPPLY, 0xAD, stream + VLX_FRAME_LEN);
    vlx_make_poll(0x21, 0x11, VLX_REG_STATUS, stream + 2 * VLX_FRAME_LEN);

    capture_t cap = {0};
    vlx_parser_t p;
    vlx_parser_init(&p, capture_cb, &cap);
    CHECK(vlx_parser_feed_buffer(&p, stream, sizeof(stream)) == 3);
    CHECK(cap.calls == 3);
    CHECK(p.stats.bytes_discarded == 0);
}

static void test_plausibility_filter(void)
{
    vlx_frame_t good = {VLX_DOMAIN, 0x11, 0x21, 0x29, 0x07, 0};
    CHECK(vlx_frame_is_plausible(&good));

    vlx_frame_t broadcast_sender = {VLX_DOMAIN, VLX_ADDR_PANELS, 0x11, 0x29, 0x07, 0};
    CHECK(!vlx_frame_is_plausible(&broadcast_sender));

    vlx_frame_t self = {VLX_DOMAIN, 0x21, 0x21, 0x29, 0x07, 0};
    CHECK(!vlx_frame_is_plausible(&self));

    vlx_frame_t unknown = {VLX_DOMAIN, 0x77, 0x11, 0x29, 0x07, 0};
    CHECK(!vlx_frame_is_plausible(&unknown));

    // A mainboard broadcasting to every panel is normal traffic.
    vlx_frame_t bcast = {VLX_DOMAIN, 0x11, VLX_ADDR_PANELS, 0x32, 0x64, 0};
    CHECK(vlx_frame_is_plausible(&bcast));
}

// ---------------------------------------------------------------------------

static void test_temperature_table(void)
{
    CHECK(vlx_temp_table(0x00) == -74);
    CHECK(vlx_temp_table(0x64) == 0);     // the zero-degree anchor
    CHECK(vlx_temp_table(0xAD) == 25);
    CHECK(vlx_temp_table(0xFF) == 100);

    // Monotonic non-decreasing across the whole range. A single transposed
    // entry would break the inverse lookup silently.
    for (unsigned i = 1; i < 256u; i++) {
        CHECK(vlx_temp_table((uint8_t)i) >= vlx_temp_table((uint8_t)(i - 1)));
    }
}

static void test_temperature_saturation(void)
{
    CHECK(vlx_temp_is_saturated(0x00));
    CHECK(vlx_temp_is_saturated(0xFF));
    CHECK(vlx_temp_is_saturated(0xF7));
    CHECK(!vlx_temp_is_saturated(0xF6));
    CHECK(!vlx_temp_is_saturated(0x64));

    // Every saturated raw value at the top reads the same degree, which is what
    // makes it saturation rather than a measurement.
    for (unsigned i = 0xF7u; i < 256u; i++) {
        CHECK(vlx_temp_table((uint8_t)i) == 100);
    }
}

static void test_temperature_inverse(void)
{
    // The mapping is many-to-one, so the round trip that has to hold is
    // degrees -> raw -> degrees, not raw -> degrees -> raw.
    for (int16_t c = -70; c <= 90; c++) {
        const uint8_t raw = vlx_temp_to_raw(c);
        CHECK(vlx_temp_table(raw) >= c);
        if (raw > 0) {
            CHECK(vlx_temp_table((uint8_t)(raw - 1)) < c);
        }
    }
    CHECK(vlx_temp_to_raw(-200) == 0x00);
    CHECK(vlx_temp_to_raw(500) == 0xFF);
}

static void test_fan_speed(void)
{
    const uint8_t codes[8] = {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};
    for (int speed = 1; speed <= 8; speed++) {
        CHECK(vlx_fan_speed_to_raw(speed) == codes[speed - 1]);
        CHECK(vlx_fan_speed_from_raw(codes[speed - 1]) == speed);
    }

    CHECK(vlx_fan_speed_to_raw(0) == 0);
    CHECK(vlx_fan_speed_to_raw(9) == 0);

    // Thermometer coding means most byte values are not speeds at all. Anything
    // that is not one of the eight must be refused, not rounded.
    unsigned valid = 0;
    for (unsigned i = 0; i < 256u; i++) {
        if (vlx_fan_speed_from_raw((uint8_t)i) != VLX_FAN_SPEED_INVALID) {
            valid++;
        }
    }
    CHECK(valid == 8);
    CHECK(vlx_fan_speed_from_raw(0x02) == VLX_FAN_SPEED_INVALID);
    CHECK(vlx_fan_speed_from_raw(0x00) == VLX_FAN_SPEED_INVALID);
}

static void test_relative_humidity(void)
{
    CHECK(vlx_rh_from_raw(0x32) == VLX_RH_INVALID);
    CHECK(vlx_rh_from_raw(0x33) == 0);
    CHECK(vlx_rh_from_raw(0xFF) == 100);
    // (0x92 - 51) / 2.04 = 46.57, rounds to 47
    CHECK(vlx_rh_from_raw(0x92) == 47);

    for (unsigned i = 0x33u; i < 256u; i++) {
        const int rh = vlx_rh_from_raw((uint8_t)i);
        CHECK(rh >= 0 && rh <= 100);
    }
}

static void test_co2_pair(void)
{
    CHECK(vlx_co2_from_bytes(0x03, 0xE8) == 1000);
    CHECK(vlx_co2_from_bytes(0x00, 0x00) == 0);
    CHECK(vlx_co2_from_bytes(0xFF, 0xFF) == 65535);
}

// ---------------------------------------------------------------------------

static void test_write_allow_list_has_one_entry(void)
{
    // This is the containment for risk R3: Vallox warns that writing an
    // incorrect register can damage the unit. If this count ever changes,
    // there had better be a measurement report next to the commit that
    // changed it.
    unsigned allowed = 0;
    for (unsigned reg = 0; reg < 256u; reg++) {
        if (vlx_register_is_write_allowed((uint8_t)reg)) {
            allowed++;
        }
    }
    CHECK(allowed == 1);
    CHECK(vlx_register_is_write_allowed(VLX_REG_FAN_SPEED));
    CHECK(!vlx_register_is_write_allowed(VLX_REG_STATUS));
    CHECK(!vlx_register_is_write_allowed(VLX_REG_HEAT_SETPOINT));
}

static void test_value_range_checks(void)
{
    CHECK(vlx_value_is_valid_for(VLX_REG_FAN_SPEED, 0x07));
    CHECK(!vlx_value_is_valid_for(VLX_REG_FAN_SPEED, 0x06));

    CHECK(vlx_value_is_valid_for(VLX_REG_HEAT_SETPOINT, vlx_temp_to_raw(20)));
    CHECK(!vlx_value_is_valid_for(VLX_REG_HEAT_SETPOINT, 0xFF));

    CHECK(vlx_value_is_valid_for(VLX_REG_DC_FAN_SUPPLY, 100));
    CHECK(vlx_value_is_valid_for(VLX_REG_DC_FAN_SUPPLY, 65));
    CHECK(!vlx_value_is_valid_for(VLX_REG_DC_FAN_SUPPLY, 64));
    CHECK(!vlx_value_is_valid_for(VLX_REG_DC_FAN_SUPPLY, 101));

    CHECK(!vlx_value_is_valid_for(VLX_REG_RH_BASIC_LEVEL, 0x32));
    CHECK(vlx_value_is_valid_for(VLX_REG_RH_BASIC_LEVEL, 0x33));
}

// ---------------------------------------------------------------------------

static void test_names(void)
{
    CHECK(strcmp(vlx_register_name(VLX_REG_FAN_SPEED), "fan_speed") == 0);
    CHECK(strcmp(vlx_register_name(VLX_POLL), "poll") == 0);
    CHECK(strcmp(vlx_register_name(0x99), "unknown") == 0);
    CHECK(strcmp(vlx_fault_name(VLX_FAULT_WATER_COIL_FROST),
                 "water_coil_frost_risk") == 0);
    CHECK(strcmp(vlx_fault_name(0xEE), "unknown") == 0);

    // Never NULL, for every possible byte: these strings go straight into
    // printf in the capture decoder and into ESP_LOG on the device.
    for (unsigned i = 0; i < 256u; i++) {
        CHECK(vlx_register_name((uint8_t)i) != NULL);
        CHECK(vlx_fault_name((uint8_t)i) != NULL);
    }
}

static void test_bus_survey_picks_a_free_address(void)
{
    vlx_bus_survey_t s;
    vlx_bus_survey_init(&s);
    CHECK(vlx_bus_survey_pick_address(&s) == 0x29);

    // Commissioning leaves the factory panels at the bottom of the range, so a
    // fourth client belongs at the top.
    const vlx_frame_t p1 = {VLX_DOMAIN, 0x21, 0x11, 0x00, 0xA3, 0};
    const vlx_frame_t p2 = {VLX_DOMAIN, 0x22, 0x11, 0x00, 0xA3, 0};
    vlx_bus_survey_observe(&s, &p1);
    vlx_bus_survey_observe(&s, &p2);
    CHECK(s.frames_seen == 2);
    CHECK(vlx_bus_survey_pick_address(&s) == 0x29);

    // A panel that only ever answers still occupies its address.
    const vlx_frame_t to_p9 = {VLX_DOMAIN, 0x11, 0x29, 0x32, 0x64, 0};
    vlx_bus_survey_observe(&s, &to_p9);
    CHECK(vlx_bus_survey_pick_address(&s) == 0x27);
}

static void test_bus_survey_never_picks_the_lon_address(void)
{
    vlx_bus_survey_t s;
    vlx_bus_survey_init(&s);
    // Occupy everything above the LON slot.
    const vlx_frame_t seen29 = {VLX_DOMAIN, 0x29, 0x11, 0x00, 0xA3, 0};
    vlx_bus_survey_observe(&s, &seen29);
    // 0x28 is the LON gateway's address and is skipped even when it is silent.
    CHECK(vlx_bus_survey_pick_address(&s) == 0x27);
}

static void test_bus_survey_reports_exhaustion(void)
{
    vlx_bus_survey_t s;
    vlx_bus_survey_init(&s);
    for (uint8_t a = VLX_ADDR_PANEL_FIRST; a <= VLX_ADDR_PANEL_LAST; a++) {
        const vlx_frame_t f = {VLX_DOMAIN, a, 0x11, 0x00, 0xA3, 0};
        vlx_bus_survey_observe(&s, &f);
    }
    CHECK(vlx_bus_survey_pick_address(&s) == 0);
}

int main(void)
{
    test_documented_example();
    test_checksum_wraps();
    test_write_is_not_a_poll();
    test_encode_recomputes_checksum();

    test_parser_accepts_a_clean_frame();
    test_parser_survives_arbitrary_chunking();
    test_parser_resyncs_after_lost_bytes();
    test_parser_rejects_bad_checksum();
    test_parser_counts_domain_rejects_separately();
    test_parser_reset_drops_partial_alignment();
    test_parser_handles_back_to_back_frames();
    test_plausibility_filter();

    test_temperature_table();
    test_temperature_saturation();
    test_temperature_inverse();
    test_fan_speed();
    test_relative_humidity();
    test_co2_pair();

    test_names();
    test_write_allow_list_has_one_entry();
    test_value_range_checks();

    test_bus_survey_picks_a_free_address();
    test_bus_survey_never_picks_the_lon_address();
    test_bus_survey_reports_exhaustion();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
