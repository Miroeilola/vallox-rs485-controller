// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Unit tests for the frame parser. A bug found here costs a minute; the same
// bug found on a live bus costs an afternoon with an oscilloscope.

#include <stdio.h>
#include <string.h>

#include "device_core.h"

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
    size_t last_len;
    uint8_t last[DEVICE_MAX_PAYLOAD];
} capture_t;

static void capture_cb(const uint8_t *payload, size_t len, void *ctx)
{
    capture_t *c = (capture_t *)ctx;
    c->calls++;
    c->last_len = len;
    memcpy(c->last, payload, len);
}

static void test_encode_decode_roundtrip(void)
{
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    uint8_t frame[16];
    const size_t n = device_encode(payload, sizeof(payload), frame, sizeof(frame));
    CHECK(n == sizeof(payload) + 3);
    CHECK(frame[0] == DEVICE_SOF);
    CHECK(frame[1] == sizeof(payload));

    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);
    CHECK(device_parser_feed_buffer(&p, frame, n) == 1);
    CHECK(cap.calls == 1);
    CHECK(cap.last_len == sizeof(payload));
    CHECK(memcmp(cap.last, payload, sizeof(payload)) == 0);
    CHECK(p.stats.frames_ok == 1);
}

static void test_split_across_reads(void)
{
    // A UART hands over arbitrary chunks; the parser must not care where the
    // boundary falls.
    const uint8_t payload[] = {0xAA, 0xBB};
    uint8_t frame[16];
    const size_t n = device_encode(payload, sizeof(payload), frame, sizeof(frame));

    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);
    for (size_t split = 1; split < n; split++) {
        device_parser_init(&p, capture_cb, &cap);
        cap.calls = 0;
        device_parser_feed_buffer(&p, frame, split);
        device_parser_feed_buffer(&p, frame + split, n - split);
        CHECK(cap.calls == 1);
    }
}

static void test_bad_checksum_is_rejected(void)
{
    const uint8_t payload[] = {0x01, 0x02};
    uint8_t frame[16];
    const size_t n = device_encode(payload, sizeof(payload), frame, sizeof(frame));
    frame[n - 1] ^= 0xFF;

    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);
    CHECK(device_parser_feed_buffer(&p, frame, n) == 0);
    CHECK(cap.calls == 0);
    CHECK(p.stats.checksum_errors == 1);
}

static void test_resync_after_garbage(void)
{
    const uint8_t payload[] = {0x77};
    uint8_t frame[16];
    const size_t n = device_encode(payload, sizeof(payload), frame, sizeof(frame));

    uint8_t stream[32];
    const uint8_t noise[] = {0xFF, 0x00, 0x5A};
    memcpy(stream, noise, sizeof(noise));
    memcpy(stream + sizeof(noise), frame, n);

    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);
    CHECK(device_parser_feed_buffer(&p, stream, sizeof(noise) + n) == 1);
    CHECK(p.stats.resyncs == sizeof(noise));
}

static void test_length_bounds(void)
{
    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);

    // Zero length and over-long length are both impossible and must not be
    // trusted enough to start buffering.
    device_parser_feed(&p, DEVICE_SOF);
    device_parser_feed(&p, 0x00);
    CHECK(p.stats.length_errors == 1);

    device_parser_feed(&p, DEVICE_SOF);
    device_parser_feed(&p, DEVICE_MAX_PAYLOAD + 1);
    CHECK(p.stats.length_errors == 2);
    CHECK(cap.calls == 0);

    uint8_t out[4];
    CHECK(device_encode((const uint8_t *)"x", 0, out, sizeof(out)) == 0);
    CHECK(device_encode((const uint8_t *)"x", 1, out, 2) == 0);
}

static void test_recovers_after_impossible_length(void)
{
    // A false SOF inside payload data is followed by whatever byte came next.
    // When that byte cannot be a length, the parser must go back to hunting
    // and still accept the next real frame.
    const uint8_t payload[] = {0x42};
    uint8_t frame[8];
    const size_t n = device_encode(payload, sizeof(payload), frame, sizeof(frame));

    capture_t cap = {0};
    device_parser_t p;
    device_parser_init(&p, capture_cb, &cap);
    device_parser_feed(&p, DEVICE_SOF);
    device_parser_feed(&p, 0xFF);         // impossible length -> back to sync
    CHECK(p.stats.length_errors == 1);
    CHECK(device_parser_feed_buffer(&p, frame, n) == 1);
    CHECK(cap.calls == 1);
}

int main(void)
{
    test_encode_decode_roundtrip();
    test_split_across_reads();
    test_bad_checksum_is_rejected();
    test_resync_after_garbage();
    test_length_bounds();
    test_recovers_after_impossible_length();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
