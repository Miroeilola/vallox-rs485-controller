// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Vallox DIGIT RS-485 protocol codec.
//
// Free of ESP-IDF headers on purpose: the same source is compiled by the
// firmware, by the ESPHome component and by the host test runner. Nothing here
// touches a UART, a clock or the network — that belongs to the layer above.
//
// The protocol is 9600 8N1 half duplex. Every telegram is six bytes:
//
//   +--------+--------+----------+----------+--------+----------+
//   | domain | sender | receiver | register | value  | checksum |
//   +--------+--------+----------+----------+--------+----------+
//      0x01                                            low byte of sum of 0..4
//
// A poll sets register = 0x00 and puts the wanted register number in value.
// A write, and the answer to a poll, put the register number in register and
// its contents in value.
//
// Every register number and encoding in this file is compiled from four
// independent implementations and the Vallox Digit2 SE manual, and none of it
// has been verified against a machine yet. See docs/research/protocol.md for
// the confidence rating of each claim. Treat this header as a hypothesis with
// tests, not as a specification.

#ifndef VALLOX_PROTOCOL_H
#define VALLOX_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VLX_FRAME_LEN   6u
#define VLX_DOMAIN      0x01u
#define VLX_POLL        0x00u   // register field value that marks a read request

// ---------------------------------------------------------------------------
// Addresses
// ---------------------------------------------------------------------------

#define VLX_ADDR_MAINBOARDS   0x10u  // broadcast to every mainboard
#define VLX_ADDR_MAINBOARD_1  0x11u  // the machine
#define VLX_ADDR_PANELS       0x20u  // broadcast to every panel
#define VLX_ADDR_PANEL_FIRST  0x21u
#define VLX_ADDR_PANEL_LAST   0x29u
#define VLX_ADDR_LON          0x28u  // the optional LON gateway module

// ---------------------------------------------------------------------------
// Registers. Grouped as in docs/research/protocol.md.
// ---------------------------------------------------------------------------

// Measurements, read only
#define VLX_REG_IO_FAN_RELAYS       0x06u  // bit image of the eight speed relays
#define VLX_REG_IO_MULTI_1          0x07u
#define VLX_REG_IO_MULTI_2          0x08u
#define VLX_REG_FAN_SPEED           0x29u
#define VLX_REG_RH_HIGHEST          0x2Au
#define VLX_REG_CO2_HIGH            0x2Bu
#define VLX_REG_CO2_LOW             0x2Cu
#define VLX_REG_CO2_SENSORS_FITTED  0x2Du
#define VLX_REG_CURRENT_MA          0x2Eu
#define VLX_REG_RH_SENSOR_1         0x2Fu
#define VLX_REG_RH_SENSOR_2         0x30u
#define VLX_REG_TEMP_OUTDOOR        0x32u
#define VLX_REG_TEMP_EXHAUST        0x33u
#define VLX_REG_TEMP_EXTRACT        0x34u
#define VLX_REG_TEMP_SUPPLY         0x35u
#define VLX_REG_FAULT               0x36u
#define VLX_REG_POST_HEAT_ON_CNT    0x55u
#define VLX_REG_POST_HEAT_OFF_CNT   0x56u
#define VLX_REG_POST_HEAT_TARGET    0x57u

// Older machines are reported to broadcast temperatures here instead. Which set
// a given machine uses is discovered from its traffic, never assumed.
#define VLX_REG_TEMP_OUTDOOR_LEGACY 0x58u
#define VLX_REG_TEMP_EXTRACT_LEGACY 0x5Au
#define VLX_REG_TEMP_SUPPLY_LEGACY  0x5Bu
#define VLX_REG_TEMP_EXHAUST_LEGACY 0x5Cu

// Flag registers
#define VLX_REG_FLAGS_1             0x6Cu
#define VLX_REG_FLAGS_2             0x6Du
#define VLX_REG_FLAGS_3             0x6Eu
#define VLX_REG_FLAGS_4             0x6Fu
#define VLX_REG_FLAGS_5             0x70u
#define VLX_REG_FLAGS_6             0x71u
#define VLX_REG_BOOST_MINUTES       0x79u

// Bus control, broadcast twice around a CO2 sensor exchange
#define VLX_REG_RESUME              0x8Fu
#define VLX_REG_SUSPEND             0x91u

// Settings
#define VLX_REG_STATUS              0xA3u
#define VLX_REG_HEAT_SETPOINT       0xA4u
#define VLX_REG_FAN_SPEED_MAX       0xA5u
#define VLX_REG_SERVICE_INTERVAL    0xA6u
#define VLX_REG_PREHEAT_SETPOINT    0xA7u
#define VLX_REG_SUPPLY_FAN_STOP     0xA8u
#define VLX_REG_FAN_SPEED_DEFAULT   0xA9u
#define VLX_REG_PROGRAM             0xAAu
#define VLX_REG_SERVICE_MONTHS_LEFT 0xABu
#define VLX_REG_RH_BASIC_LEVEL      0xAEu
#define VLX_REG_BYPASS_SETPOINT     0xAFu
#define VLX_REG_DC_FAN_SUPPLY       0xB0u
#define VLX_REG_DC_FAN_EXHAUST      0xB1u
#define VLX_REG_DEFROST_HYSTERESIS  0xB2u
#define VLX_REG_CO2_SETPOINT_HIGH   0xB3u
#define VLX_REG_CO2_SETPOINT_LOW    0xB4u
#define VLX_REG_PROGRAM_2           0xB5u

// Bits of VLX_REG_STATUS (0xA3). Bits 0..3 are settable, 4..7 are indicators.
#define VLX_STATUS_POWER        (1u << 0)
#define VLX_STATUS_CO2_CONTROL  (1u << 1)
#define VLX_STATUS_RH_CONTROL   (1u << 2)
#define VLX_STATUS_WINTER_MODE  (1u << 3)  // 1 = heat recovery, 0 = summer bypass
#define VLX_STATUS_FILTER       (1u << 4)
#define VLX_STATUS_HEATING      (1u << 5)
#define VLX_STATUS_FAULT        (1u << 6)
#define VLX_STATUS_SERVICE      (1u << 7)

// Bits of VLX_REG_IO_MULTI_2 (0x08)
#define VLX_IO2_DAMPER_SUMMER   (1u << 1)
#define VLX_IO2_FAULT_RELAY     (1u << 2)
#define VLX_IO2_SUPPLY_FAN_OFF  (1u << 3)
#define VLX_IO2_PREHEATING      (1u << 4)
#define VLX_IO2_EXHAUST_FAN_OFF (1u << 5)
#define VLX_IO2_BOOST_SWITCH    (1u << 6)

// Bits of VLX_REG_FLAGS_6 (0x71)
#define VLX_F6_REMOTE_CONTROL   (1u << 4)
#define VLX_F6_BOOST_ACTIVATE   (1u << 5)  // read, set this bit, write back
#define VLX_F6_BOOST_RUNNING    (1u << 6)

// Fault numbers reported in VLX_REG_FAULT (0x36)
typedef enum {
    VLX_FAULT_NONE                 = 0x00,
    VLX_FAULT_SUPPLY_AIR_SENSOR    = 0x05,
    VLX_FAULT_CO2_ALARM            = 0x06,
    VLX_FAULT_OUTDOOR_AIR_SENSOR   = 0x07,
    VLX_FAULT_EXTRACT_AIR_SENSOR   = 0x08,
    VLX_FAULT_WATER_COIL_FROST     = 0x09,
    VLX_FAULT_EXHAUST_AIR_SENSOR   = 0x0A,
} vlx_fault_t;

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

typedef struct {
    uint8_t domain;
    uint8_t sender;
    uint8_t receiver;
    uint8_t reg;
    uint8_t value;
    uint8_t checksum;
} vlx_frame_t;

// Low byte of the sum of the first five bytes.
uint8_t vlx_checksum(const uint8_t raw[VLX_FRAME_LEN]);

// Decode six bytes. Returns false if the domain byte or the checksum is wrong;
// out is still filled so a caller can log what was rejected.
bool vlx_frame_decode(const uint8_t raw[VLX_FRAME_LEN], vlx_frame_t *out);

// Serialise a frame, recomputing the checksum. The checksum field of f is
// ignored on input and updated on return.
void vlx_frame_encode(vlx_frame_t *f, uint8_t out[VLX_FRAME_LEN]);

// Build a read request for reg, and a write of value into reg.
void vlx_make_poll(uint8_t sender, uint8_t receiver, uint8_t reg,
                   uint8_t out[VLX_FRAME_LEN]);
void vlx_make_write(uint8_t sender, uint8_t receiver, uint8_t reg, uint8_t value,
                    uint8_t out[VLX_FRAME_LEN]);

// True when the frame is a read request rather than data.
bool vlx_frame_is_poll(const vlx_frame_t *f);

// A six-byte frame has no start delimiter, so a checksum can pass by accident
// at the wrong byte alignment — roughly once in 65 000 random alignments once
// the domain byte is taken into account. This is the cheap second filter: the
// sender and receiver have to be addresses that exist on this bus.
bool vlx_frame_is_plausible(const vlx_frame_t *f);

bool vlx_addr_is_panel(uint8_t addr);
bool vlx_addr_is_mainboard(uint8_t addr);

// ---------------------------------------------------------------------------
// Byte-stream parser
// ---------------------------------------------------------------------------
//
// Resynchronising: with no start delimiter the only way back into alignment
// after a lost byte is to slide one byte at a time until a frame validates.
//
// The parser is deliberately blind to time. Real framing on this bus also comes
// from the gap between telegrams, and the layer that owns the UART should call
// vlx_parser_reset() when it sees an idle gap — that turns an ambiguous
// alignment into a certain one and costs nothing.

typedef struct {
    unsigned frames_ok;
    unsigned domain_rejects;    // alignment discarded because byte 0 was not 0x01
    unsigned checksum_rejects;  // alignment discarded on a checksum mismatch
    unsigned bytes_discarded;   // bytes dropped while hunting for alignment
} vlx_stats_t;

typedef void (*vlx_frame_cb_t)(const vlx_frame_t *frame, void *ctx);

typedef struct {
    uint8_t buf[VLX_FRAME_LEN];
    uint8_t fill;
    vlx_frame_cb_t on_frame;
    void *ctx;
    vlx_stats_t stats;
} vlx_parser_t;

// on_frame may be NULL, in which case frames are counted but discarded.
void vlx_parser_init(vlx_parser_t *p, vlx_frame_cb_t on_frame, void *ctx);

// Drop any partial alignment. Call this after an idle gap on the bus.
void vlx_parser_reset(vlx_parser_t *p);

// Feed one byte. Returns true when this byte completed a valid frame.
bool vlx_parser_feed(vlx_parser_t *p, uint8_t byte);

// Feed a buffer. Returns the number of complete frames produced.
unsigned vlx_parser_feed_buffer(vlx_parser_t *p, const uint8_t *data, size_t len);

// ---------------------------------------------------------------------------
// Value encodings
// ---------------------------------------------------------------------------

// Temperatures are raw NTC 5 kOhm counts through a fixed 256-entry table.
// vlx_temp_table() is the faithful lookup and always succeeds.
int16_t vlx_temp_table(uint8_t raw);

// The table saturates at both ends: everything from 0xF7 upwards reads 100 degC
// and 0x00 reads -74 degC. Those are the sensor at its rail — an open or shorted
// probe — not a measurement, so they must not be published as one.
bool vlx_temp_is_saturated(uint8_t raw);

// Inverse lookup for writing a setpoint. Several raw values map to the same
// degree, so this returns the lowest raw value that reaches celsius, clamped to
// the ends of the table.
uint8_t vlx_temp_to_raw(int16_t celsius);

// Fan speed is thermometer coded: 1 -> 0x01, 2 -> 0x03 ... 8 -> 0xFF. Anything
// else is not a speed.
#define VLX_FAN_SPEED_INVALID 0
int vlx_fan_speed_from_raw(uint8_t raw);   // 1..8, or VLX_FAN_SPEED_INVALID
uint8_t vlx_fan_speed_to_raw(int speed);   // 0 when speed is outside 1..8

// Relative humidity: (raw - 51) / 2.04 percent, defined from raw 0x33 upwards.
#define VLX_RH_INVALID (-1)
int vlx_rh_from_raw(uint8_t raw);

// CO2 is a big-endian pair split across two registers.
uint16_t vlx_co2_from_bytes(uint8_t high, uint8_t low);

// ---------------------------------------------------------------------------
// Write allow-list
// ---------------------------------------------------------------------------
//
// Vallox states that writing an incorrect register or an out-of-range value can
// damage the unit, so this list is the containment for that risk and it is not
// a convenience. A register joins it only when a measurement report in
// docs/measurements/ shows it behaving as expected on real hardware.
//
// Today the list has exactly one entry.
bool vlx_register_is_write_allowed(uint8_t reg);

// Range check for a value about to be written, matching the register's
// encoding. Returns false for anything the encoding cannot represent.
bool vlx_value_is_valid_for(uint8_t reg, uint8_t value);

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------
//
// For logs, the device's status output and the capture decoder. Kept here so
// there is one table rather than three that drift.

// Short name of a register, or "unknown" for anything not in the map. Never
// returns NULL.
const char *vlx_register_name(uint8_t reg);

// Name of a fault number from VLX_REG_FAULT. Never returns NULL.
const char *vlx_fault_name(uint8_t fault);

// ---------------------------------------------------------------------------
// Bus survey
// ---------------------------------------------------------------------------
//
// Existing implementations pick their bus address by convention and hope it is
// free. Two clients on one address put the machine into a bus-fault state, so
// this listens first and picks an address nobody is using.

typedef struct {
    uint16_t panel_seen;   // bit n set means address 0x21+n has been heard
    unsigned frames_seen;
} vlx_bus_survey_t;

void vlx_bus_survey_init(vlx_bus_survey_t *s);
void vlx_bus_survey_observe(vlx_bus_survey_t *s, const vlx_frame_t *f);

// Highest unused panel address, so the pick lands away from the factory panels.
// The manufacturer's commissioning procedure ends with the panels holding
// addresses 1, 2 and 3, so they occupy the bottom of the range and the top is
// where a fourth client belongs. Returns 0 when every address is taken. The LON
// address is never picked.
uint8_t vlx_bus_survey_pick_address(const vlx_bus_survey_t *s);

#ifdef __cplusplus
}
#endif

#endif  // VALLOX_PROTOCOL_H
