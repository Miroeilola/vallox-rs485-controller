// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Emulator of the Vallox DIGIT mainboard as seen from the RS-485 bus: a
// register map, the poll/write/acknowledge behaviour and the periodic
// broadcasts from docs/research/protocol.md, plus a coarse thermal model so the
// readings move when the settings do. It is itself a bus device at 0x11 and
// speaks in real six-byte frames through whatever transport the host gives it.
//
// It is exactly as right as the protocol document. Nothing here has been
// verified against a machine; every behaviour names its confidence class.
//
// Free of ESP-IDF headers: compiled by the host tests and the browser
// simulator (Emscripten). Not used on the device.

#ifndef VALLOX_MACHINE_H
#define VALLOX_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vallox_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VLX_CONF_MANUFACTURER,     // in Vallox's own protocol document
    VLX_CONF_IMPLEMENTATIONS,  // agreed by the four independent implementations
    VLX_CONF_ASSUMED,          // one source, or inferred
} vlx_conf_t;

typedef struct {
    uint8_t    reg;
    bool       writable;
    uint8_t    def;
    vlx_conf_t conf;
} vlx_machine_reg_t;

#define VLX_MACHINE_NEVER 0xFFFFu      // reply_delay_ms: never answer a poll

// Thermal model parameters. Temperatures in degrees C (float is fine here: this
// code does not run on the MCU).
typedef struct {
    float t_outdoor;        // set from the side panel / test; default 5
    float t_indoor;         // what the rooms sit at; default 21
    float efficiency;       // heat-recovery efficiency 0..1; default 0.6
    float tau_s;            // time constant at fan speed 1, seconds; default 600
    float time_scale;       // 1 = real time, 10, 60 for demos; default 1
} vlx_machine_params_t;

typedef struct {
    // registers
    uint8_t regs[256];
    bool    known[256];
    bool    writable[256];
    vlx_conf_t conf[256];
    // protocol behaviour
    uint16_t reply_delay_ms;        // 0 default; 10 / 200 / VLX_MACHINE_NEVER for tests.
                                     // Counts from the tick that hears the frame, not
                                     // from the moment it was fed.
    uint32_t broadcast_period_ms;   // 12000
    uint32_t broadcast_spacing_ms;  // 130 between the frames of one broadcast round
    // internal state
    vlx_parser_t parser;
    uint8_t  pending[64];           // bytes queued to go out (answers, acks)
    size_t   pending_len;
    uint32_t pending_due_ms;        // not before this time
    bool     pending_armed;
    bool     pending_scheduled;     // pending_due_ms has been set for the current batch
    uint32_t last_broadcast_ms;
    uint8_t  broadcast_idx;         // index into the broadcast set during a round
    uint32_t next_broadcast_frame_ms;
    uint32_t last_tick_ms;
    bool     have_tick;
    // physics
    vlx_machine_params_t p;
    float t_extract, t_supply, t_exhaust;   // continuous state; regs hold the NTC-rounded copy
    uint32_t service_elapsed_ms;
} vlx_machine_t;

void   vlx_machine_init(vlx_machine_t *m);

// Bytes heard on the bus (anything: our own echo is filtered by address).
void   vlx_machine_feed(vlx_machine_t *m, const uint8_t *bytes, size_t n);

// Advance time, run the physics, and write whatever the machine sends now
// (poll answers, acknowledges, broadcasts) into out. Returns bytes written.
size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max);

// Register access for tests and the simulator side panel.
// vlx_machine_reg_set() only ever changes a register already known from the
// register table; a write to an unknown register is a no-op and does NOT
// teach the model that register — the same silence a real write would get.
bool    vlx_machine_reg_known(const vlx_machine_t *m, uint8_t reg);
uint8_t vlx_machine_reg_get(const vlx_machine_t *m, uint8_t reg);
void    vlx_machine_reg_set(vlx_machine_t *m, uint8_t reg, uint8_t value);

// Confidence class of a register's behaviour, from the register table.
// Returns VLX_CONF_ASSUMED for a register the model does not know.
vlx_conf_t vlx_machine_reg_conf(const vlx_machine_t *m, uint8_t reg);

// Fault injection: sets VLX_REG_FAULT and the fault bit; cleared by
// vlx_machine_fault_clear() or by the panel writing 0 to VLX_REG_FAULT
// (the clearing write is ASSUMED — no document describes how the panel
// acknowledges a fault).
void vlx_machine_fault(vlx_machine_t *m, vlx_fault_t code);
void vlx_machine_fault_clear(vlx_machine_t *m);

// Physics step, called from tick; exposed for the physics tests.
void vlx_machine_physics_step(vlx_machine_t *m, float dt_s);

#ifdef __cplusplus
}
#endif
#endif
