// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet

#include "device_core.h"

#include <string.h>

void device_parser_init(device_parser_t *p, device_frame_cb_t on_frame, void *ctx)
{
    memset(p, 0, sizeof(*p));
    p->state = DEVICE_STATE_SYNC;
    p->on_frame = on_frame;
    p->ctx = ctx;
}

uint8_t device_checksum(uint8_t len, const uint8_t *payload)
{
    uint8_t sum = len;
    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + payload[i]);
    }
    return sum;
}

bool device_parser_feed(device_parser_t *p, uint8_t byte)
{
    switch (p->state) {
    case DEVICE_STATE_SYNC:
        if (byte == DEVICE_SOF) {
            p->state = DEVICE_STATE_LENGTH;
        } else {
            p->stats.resyncs++;
        }
        return false;

    case DEVICE_STATE_LENGTH:
        if (byte == 0 || byte > DEVICE_MAX_PAYLOAD) {
            // An impossible length means the preceding SOF was payload data,
            // not a real start of frame. Drop back to hunting rather than
            // buffering against a length we do not believe.
            p->stats.length_errors++;
            p->state = DEVICE_STATE_SYNC;
            return false;
        }
        p->expected = byte;
        p->received = 0;
        p->checksum = byte;
        p->state = DEVICE_STATE_PAYLOAD;
        return false;

    case DEVICE_STATE_PAYLOAD:
        p->buffer[p->received++] = byte;
        p->checksum = (uint8_t)(p->checksum + byte);
        if (p->received == p->expected) {
            p->state = DEVICE_STATE_CHECKSUM;
        }
        return false;

    case DEVICE_STATE_CHECKSUM:
        p->state = DEVICE_STATE_SYNC;
        if (byte != p->checksum) {
            p->stats.checksum_errors++;
            return false;
        }
        p->stats.frames_ok++;
        if (p->on_frame) {
            p->on_frame(p->buffer, p->expected, p->ctx);
        }
        return true;
    }
    return false;
}

unsigned device_parser_feed_buffer(device_parser_t *p, const uint8_t *data, size_t len)
{
    unsigned frames = 0;
    for (size_t i = 0; i < len; i++) {
        if (device_parser_feed(p, data[i])) {
            frames++;
        }
    }
    return frames;
}

size_t device_encode(const uint8_t *payload, uint8_t len, uint8_t *out, size_t out_size)
{
    if (len == 0 || len > DEVICE_MAX_PAYLOAD) return 0;
    const size_t needed = (size_t)len + 3u;
    if (out_size < needed) return 0;

    out[0] = DEVICE_SOF;
    out[1] = len;
    memcpy(&out[2], payload, len);
    out[needed - 1] = device_checksum(len, payload);
    return needed;
}
