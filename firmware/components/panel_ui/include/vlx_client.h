// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The panel's side of the bus: a shadow copy of the machine's registers with
// timestamps, filled by polls and by the machine's broadcasts; writes with the
// one-byte acknowledge; a bus-fault state after ten unanswered polls (claim 24).
// The UI draws from the shadow and never touches the bus itself.
#ifndef VLX_CLIENT_H
#define VLX_CLIENT_H
#include <stdbool.h>
#include <stdint.h>
#include "vallox_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VLX_CLIENT_POLL_PERIOD_MS   250u
#define VLX_CLIENT_FAULT_RETRY_MS   1000u
#define VLX_CLIENT_RESPONSE_MS      50u     // the machine promises 10 ms; two 20 ms ticks of slack
#define VLX_CLIENT_WRITE_TRIES      3u
#define VLX_CLIENT_FAULT_AFTER      10u
#define VLX_CLIENT_ALIVE_MS         2000u
#define VLX_CLIENT_STALE_MS         30000u

typedef enum { VLX_WRITE_IDLE, VLX_WRITE_PENDING, VLX_WRITE_ACKED, VLX_WRITE_FAILED } vlx_write_state_t;
typedef enum { VLX_WRITE_QUEUED, VLX_WRITE_REFUSED, VLX_WRITE_BUSY } vlx_write_result_t;

typedef struct {
    uint8_t  my_addr;
    bool     tx_enabled;
    // shadow
    uint8_t  val[256];
    uint32_t ts[256];
    bool     seen[256];
    // polling
    const uint8_t *poll_list;
    uint8_t  poll_n, poll_idx;
    uint32_t next_poll_ms;
    bool     poll_pending;
    uint8_t  poll_reg;
    uint32_t poll_sent_ms;
    uint8_t  unanswered;
    bool     bus_fault;
    // writes
    vlx_write_state_t write_state;
    bool     write_queued;           // queued, not yet on the wire
    uint8_t  write_reg, write_val, write_csum, write_tries;
    uint32_t write_sent_ms;
    uint8_t  poll_next_override;     // register to poll right after an ack (0 = none)
    // liveness
    bool     heard_any;
    uint32_t last_heard_ms;
    uint32_t now_ms;                 // set by tick; used by the parser callback
    // stats (for the status page and tests)
    uint32_t stats_frames, stats_polls, stats_timeouts, stats_acks, stats_write_tries;
    vlx_parser_t parser;
} vlx_client_t;

extern const uint8_t vlx_client_default_poll[11];

void vlx_client_init(vlx_client_t *c, uint8_t my_addr);
void vlx_client_set_poll_list(vlx_client_t *c, const uint8_t *regs, uint8_t n);
void vlx_client_set_tx_enabled(vlx_client_t *c, bool en);
void vlx_client_tick(vlx_client_t *c, uint32_t now_ms);

bool vlx_client_get(const vlx_client_t *c, uint8_t reg, uint8_t *val, uint32_t *age_ms, uint32_t now_ms);
bool vlx_client_is_stale(const vlx_client_t *c, uint8_t reg, uint32_t now_ms);

vlx_write_result_t vlx_client_write(vlx_client_t *c, uint8_t reg, uint8_t val);
vlx_write_state_t  vlx_client_write_state(const vlx_client_t *c);
void               vlx_client_write_clear(vlx_client_t *c);

bool vlx_client_bus_ok(const vlx_client_t *c, uint32_t now_ms);
bool vlx_client_bus_fault(const vlx_client_t *c);

#ifdef __cplusplus
}
#endif
#endif
