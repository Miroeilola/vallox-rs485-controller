// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Transport-agnostic frame parser.
//
// Deliberately free of ESP-IDF headers: the same source is compiled by the
// firmware and by the host test runner. Anything that needs the network, the
// filesystem or a clock belongs in the application layer, not here.
//
// Frame format (adapt to the target protocol before use):
//
//   +------+-----+---------+----------+
//   | 0x01 | LEN | PAYLOAD | CHECKSUM |
//   +------+-----+---------+----------+
//
//   LEN      number of payload bytes, 1..DEVICE_MAX_PAYLOAD
//   CHECKSUM sum of LEN and PAYLOAD bytes, truncated to 8 bits

#ifndef DEVICE_CORE_H
#define DEVICE_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_SOF          0x01u
#define DEVICE_MAX_PAYLOAD  64u

typedef void (*device_frame_cb_t)(const uint8_t *payload, size_t len, void *ctx);

typedef enum {
    DEVICE_STATE_SYNC = 0,
    DEVICE_STATE_LENGTH,
    DEVICE_STATE_PAYLOAD,
    DEVICE_STATE_CHECKSUM,
} device_state_t;

typedef struct {
    unsigned frames_ok;        // accepted frames
    unsigned checksum_errors;  // frames dropped on checksum mismatch
    unsigned length_errors;    // frames dropped on an out-of-range length byte
    unsigned resyncs;          // times the parser dropped bytes to find a new SOF
} device_stats_t;

typedef struct {
    device_state_t state;
    uint8_t buffer[DEVICE_MAX_PAYLOAD];
    uint8_t expected;
    uint8_t received;
    uint8_t checksum;
    device_frame_cb_t on_frame;
    void *ctx;
    device_stats_t stats;
} device_parser_t;

// Initialise a parser. on_frame may be NULL, in which case frames are counted
// but discarded.
void device_parser_init(device_parser_t *p, device_frame_cb_t on_frame, void *ctx);

// Feed one received byte. Returns true when this byte completed a valid frame.
bool device_parser_feed(device_parser_t *p, uint8_t byte);

// Feed a buffer. Returns the number of complete frames produced.
unsigned device_parser_feed_buffer(device_parser_t *p, const uint8_t *data, size_t len);

// Compute the checksum over a length byte and its payload.
uint8_t device_checksum(uint8_t len, const uint8_t *payload);

// Encode a frame into out. Returns the encoded length, or 0 if the payload is
// too long or out_size is insufficient.
size_t device_encode(const uint8_t *payload, uint8_t len, uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif  // DEVICE_CORE_H
