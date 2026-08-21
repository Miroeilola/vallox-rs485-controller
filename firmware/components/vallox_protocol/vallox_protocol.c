// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet

#include "vallox_protocol.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------

// NTC 5 kOhm raw count to degrees Celsius. Identical in three independent
// implementations, which is the strongest agreement in the reverse-engineered
// material. Non-linear, and saturated at both ends.
static const int16_t k_temp[256] = {
    -74, -70, -66, -62, -59, -56, -54, -52, -50, -48, -47, -46, -44, -43, -42, -41,
    -40, -39, -38, -37, -36, -35, -34, -33, -33, -32, -31, -30, -30, -29, -28, -28,
    -27, -27, -26, -25, -25, -24, -24, -23, -23, -22, -22, -21, -21, -20, -20, -19,
    -19, -19, -18, -18, -17, -17, -16, -16, -16, -15, -15, -14, -14, -14, -13, -13,
    -12, -12, -12, -11, -11, -11, -10, -10,  -9,  -9,  -9,  -8,  -8,  -8,  -7,  -7,
     -7,  -6,  -6,  -6,  -5,  -5,  -5,  -4,  -4,  -4,  -3,  -3,  -3,  -2,  -2,  -2,
     -1,  -1,  -1,  -1,   0,   0,   0,   1,   1,   1,   2,   2,   2,   3,   3,   3,
      4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,
      9,   9,   9,  10,  10,  10,  11,  11,  11,  12,  12,  12,  13,  13,  13,  14,
     14,  14,  15,  15,  15,  16,  16,  16,  17,  17,  18,  18,  18,  19,  19,  19,
     20,  20,  21,  21,  21,  22,  22,  22,  23,  23,  24,  24,  24,  25,  25,  26,
     26,  27,  27,  27,  28,  28,  29,  29,  30,  30,  31,  31,  32,  32,  33,  33,
     34,  34,  35,  35,  36,  36,  37,  37,  38,  38,  39,  40,  40,  41,  41,  42,
     43,  43,  44,  45,  45,  46,  47,  48,  48,  49,  50,  51,  52,  53,  53,  54,
     55,  56,  57,  59,  60,  61,  62,  63,  65,  66,  68,  69,  71,  73,  75,  77,
     79,  81,  82,  86,  90,  93,  97, 100, 100, 100, 100, 100, 100, 100, 100, 100,
};

// First raw value that reads 100 degC. Everything above it reads the same, so
// the sensor is at its rail rather than measuring.
#define VLX_TEMP_SATURATED_HIGH 0xF7u

// Thermometer code, speed 1..8.
static const uint8_t k_fan[8] = {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

uint8_t vlx_checksum(const uint8_t raw[VLX_FRAME_LEN])
{
    unsigned sum = 0;
    for (unsigned i = 0; i < VLX_FRAME_LEN - 1u; i++) {
        sum += raw[i];
    }
    return (uint8_t)(sum & 0xFFu);
}

bool vlx_frame_decode(const uint8_t raw[VLX_FRAME_LEN], vlx_frame_t *out)
{
    out->domain   = raw[0];
    out->sender   = raw[1];
    out->receiver = raw[2];
    out->reg      = raw[3];
    out->value    = raw[4];
    out->checksum = raw[5];
    return out->domain == VLX_DOMAIN && out->checksum == vlx_checksum(raw);
}

void vlx_frame_encode(vlx_frame_t *f, uint8_t out[VLX_FRAME_LEN])
{
    out[0] = f->domain;
    out[1] = f->sender;
    out[2] = f->receiver;
    out[3] = f->reg;
    out[4] = f->value;
    out[5] = vlx_checksum(out);
    f->checksum = out[5];
}

void vlx_make_poll(uint8_t sender, uint8_t receiver, uint8_t reg,
                   uint8_t out[VLX_FRAME_LEN])
{
    // A read puts VLX_POLL in the register field and the wanted register in the
    // value field. Getting this the wrong way round writes to the machine.
    vlx_frame_t f = {VLX_DOMAIN, sender, receiver, VLX_POLL, reg, 0};
    vlx_frame_encode(&f, out);
}

void vlx_make_write(uint8_t sender, uint8_t receiver, uint8_t reg, uint8_t value,
                    uint8_t out[VLX_FRAME_LEN])
{
    vlx_frame_t f = {VLX_DOMAIN, sender, receiver, reg, value, 0};
    vlx_frame_encode(&f, out);
}

bool vlx_frame_is_poll(const vlx_frame_t *f)
{
    return f->reg == VLX_POLL;
}

bool vlx_addr_is_panel(uint8_t addr)
{
    return addr >= VLX_ADDR_PANEL_FIRST && addr <= VLX_ADDR_PANEL_LAST;
}

bool vlx_addr_is_mainboard(uint8_t addr)
{
    // Only mainboard 1 is reachable in practice; the range exists in the
    // addressing scheme and cheap to accept.
    return addr > VLX_ADDR_MAINBOARDS && addr < VLX_ADDR_PANELS;
}

static bool addr_is_known(uint8_t addr)
{
    return addr == VLX_ADDR_MAINBOARDS || addr == VLX_ADDR_PANELS ||
           vlx_addr_is_mainboard(addr) || vlx_addr_is_panel(addr);
}

bool vlx_frame_is_plausible(const vlx_frame_t *f)
{
    if (!addr_is_known(f->sender) || !addr_is_known(f->receiver)) {
        return false;
    }
    // Nothing on this bus talks to itself, and a broadcast is never a sender.
    if (f->sender == f->receiver) {
        return false;
    }
    return f->sender != VLX_ADDR_MAINBOARDS && f->sender != VLX_ADDR_PANELS;
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

void vlx_parser_init(vlx_parser_t *p, vlx_frame_cb_t on_frame, void *ctx)
{
    memset(p, 0, sizeof(*p));
    p->on_frame = on_frame;
    p->ctx = ctx;
}

void vlx_parser_reset(vlx_parser_t *p)
{
    p->fill = 0;
}

bool vlx_parser_feed(vlx_parser_t *p, uint8_t byte)
{
    p->buf[p->fill++] = byte;
    if (p->fill < VLX_FRAME_LEN) {
        return false;
    }

    vlx_frame_t frame;
    if (vlx_frame_decode(p->buf, &frame)) {
        p->fill = 0;
        p->stats.frames_ok++;
        if (p->on_frame != NULL) {
            p->on_frame(&frame, p->ctx);
        }
        return true;
    }

    // This alignment is not a frame. Count why, drop the oldest byte and try
    // again one position along as the next byte arrives.
    if (p->buf[0] != VLX_DOMAIN) {
        p->stats.domain_rejects++;
    } else {
        p->stats.checksum_rejects++;
    }
    p->stats.bytes_discarded++;
    memmove(p->buf, p->buf + 1, VLX_FRAME_LEN - 1u);
    p->fill = VLX_FRAME_LEN - 1u;
    return false;
}

unsigned vlx_parser_feed_buffer(vlx_parser_t *p, const uint8_t *data, size_t len)
{
    unsigned frames = 0;
    for (size_t i = 0; i < len; i++) {
        if (vlx_parser_feed(p, data[i])) {
            frames++;
        }
    }
    return frames;
}

// ---------------------------------------------------------------------------
// Value encodings
// ---------------------------------------------------------------------------

int16_t vlx_temp_table(uint8_t raw)
{
    return k_temp[raw];
}

bool vlx_temp_is_saturated(uint8_t raw)
{
    return raw == 0x00u || raw >= VLX_TEMP_SATURATED_HIGH;
}

uint8_t vlx_temp_to_raw(int16_t celsius)
{
    for (unsigned i = 0; i < 256u; i++) {
        if (k_temp[i] >= celsius) {
            return (uint8_t)i;
        }
    }
    return 0xFFu;
}

int vlx_fan_speed_from_raw(uint8_t raw)
{
    for (unsigned i = 0; i < 8u; i++) {
        if (k_fan[i] == raw) {
            return (int)i + 1;
        }
    }
    return VLX_FAN_SPEED_INVALID;
}

uint8_t vlx_fan_speed_to_raw(int speed)
{
    if (speed < 1 || speed > 8) {
        return 0;
    }
    return k_fan[speed - 1];
}

int vlx_rh_from_raw(uint8_t raw)
{
    if (raw < 0x33u) {
        return VLX_RH_INVALID;
    }
    // (raw - 51) / 2.04, rounded, in integer arithmetic: multiply by 100/204.
    const int numerator = ((int)raw - 51) * 100;
    return (numerator + 102) / 204;
}

uint16_t vlx_co2_from_bytes(uint8_t high, uint8_t low)
{
    return (uint16_t)(((uint16_t)high << 8) | low);
}

// ---------------------------------------------------------------------------
// Write allow-list
// ---------------------------------------------------------------------------

bool vlx_register_is_write_allowed(uint8_t reg)
{
    // One entry. Adding a second one without a measurement report behind it is
    // the mistake this function exists to prevent — see docs/research/risks.md,
    // risk R3.
    switch (reg) {
    case VLX_REG_FAN_SPEED:
        return true;
    default:
        return false;
    }
}

bool vlx_value_is_valid_for(uint8_t reg, uint8_t value)
{
    switch (reg) {
    case VLX_REG_FAN_SPEED:
    case VLX_REG_FAN_SPEED_MAX:
    case VLX_REG_FAN_SPEED_DEFAULT:
        return vlx_fan_speed_from_raw(value) != VLX_FAN_SPEED_INVALID;

    case VLX_REG_HEAT_SETPOINT:
    case VLX_REG_PREHEAT_SETPOINT:
    case VLX_REG_SUPPLY_FAN_STOP:
    case VLX_REG_BYPASS_SETPOINT:
        // A setpoint at the table's rail is a value the sensor scale cannot
        // represent, so it is refused rather than clamped silently.
        return !vlx_temp_is_saturated(value);

    case VLX_REG_DC_FAN_SUPPLY:
    case VLX_REG_DC_FAN_EXHAUST:
        return value >= 65u && value <= 100u;

    case VLX_REG_RH_BASIC_LEVEL:
        return value >= 0x33u;

    default:
        // Unknown encoding means no opinion. The allow-list is what stops the
        // write, not this function.
        return true;
    }
}

// ---------------------------------------------------------------------------
// Bus survey
// ---------------------------------------------------------------------------

void vlx_bus_survey_init(vlx_bus_survey_t *s)
{
    memset(s, 0, sizeof(*s));
}

void vlx_bus_survey_observe(vlx_bus_survey_t *s, const vlx_frame_t *f)
{
    s->frames_seen++;

    // A panel address counts as taken whether it speaks or is spoken to. A
    // panel that only answers polls would otherwise look free.
    if (vlx_addr_is_panel(f->sender)) {
        s->panel_seen |= (uint16_t)(1u << (f->sender - VLX_ADDR_PANEL_FIRST));
    }
    if (vlx_addr_is_panel(f->receiver)) {
        s->panel_seen |= (uint16_t)(1u << (f->receiver - VLX_ADDR_PANEL_FIRST));
    }
}

uint8_t vlx_bus_survey_pick_address(const vlx_bus_survey_t *s)
{
    for (unsigned i = 0; i <= VLX_ADDR_PANEL_LAST - VLX_ADDR_PANEL_FIRST; i++) {
        const unsigned addr = VLX_ADDR_PANEL_LAST - i;
        if (addr == VLX_ADDR_LON) {
            continue;
        }
        const unsigned bit = 1u << (addr - VLX_ADDR_PANEL_FIRST);
        if ((s->panel_seen & bit) == 0u) {
            return (uint8_t)addr;
        }
    }
    return 0;
}
