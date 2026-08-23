// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "vlx_client.h"
#include <string.h>
#include "panel_hal.h"

const uint8_t vlx_client_default_poll[11] = {
    VLX_REG_FAN_SPEED, VLX_REG_TEMP_OUTDOOR, VLX_REG_TEMP_EXHAUST, VLX_REG_TEMP_EXTRACT,
    VLX_REG_TEMP_SUPPLY, VLX_REG_STATUS, VLX_REG_HEAT_SETPOINT, VLX_REG_FAULT,
    VLX_REG_BOOST_MINUTES, VLX_REG_SERVICE_MONTHS_LEFT, VLX_REG_IO_MULTI_2,
};

static void on_frame(const vlx_frame_t *f, void *ctx)
{
    vlx_client_t *c = (vlx_client_t *)ctx;
    c->stats_frames++;
    c->heard_any = true;
    c->last_heard_ms = c->now_ms;
    if (f->sender != VLX_ADDR_MAINBOARD_1) return;
    if (f->receiver != c->my_addr && f->receiver != VLX_ADDR_PANELS) return;
    if (vlx_frame_is_poll(f)) return;                       // the machine polling us: not modelled here
    c->val[f->reg] = f->value;
    c->ts[f->reg] = c->now_ms;
    c->seen[f->reg] = true;
    if (c->poll_pending && f->reg == c->poll_reg) {
        c->poll_pending = false;
        c->unanswered = 0;
        c->bus_fault = false;
    }
}

void vlx_client_init(vlx_client_t *c, uint8_t my_addr)
{
    memset(c, 0, sizeof *c);
    c->my_addr = my_addr;
    c->tx_enabled = true;
    c->poll_list = vlx_client_default_poll;
    c->poll_n = (uint8_t)(sizeof vlx_client_default_poll / sizeof vlx_client_default_poll[0]);
    vlx_parser_init(&c->parser, on_frame, c);
}

void vlx_client_set_poll_list(vlx_client_t *c, const uint8_t *regs, uint8_t n)
{
    c->poll_list = regs;
    c->poll_n = regs ? n : 0;
    c->poll_idx = 0;
}

void vlx_client_set_tx_enabled(vlx_client_t *c, bool en)
{
    c->tx_enabled = en;
    if (!en && (c->write_state == VLX_WRITE_PENDING || c->write_queued)) {
        c->write_state = VLX_WRITE_FAILED;
        c->write_queued = false;
        c->poll_pending = false;      // a stale poll must not be "answered" by a late reply
    }
}

static void send_write(vlx_client_t *c, uint32_t now_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_write(c->my_addr, VLX_ADDR_MAINBOARD_1, c->write_reg, c->write_val, f);
    c->write_csum = f[5];
    hal_bus_write(f, VLX_FRAME_LEN);
    c->write_sent_ms = now_ms;
    c->write_tries++;
    c->stats_write_tries++;
    c->write_state = VLX_WRITE_PENDING;
    c->write_queued = false;
}

static void send_poll(vlx_client_t *c, uint8_t reg, uint32_t now_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_poll(c->my_addr, VLX_ADDR_MAINBOARD_1, reg, f);
    hal_bus_write(f, VLX_FRAME_LEN);
    c->poll_pending = true;
    c->poll_reg = reg;
    c->poll_sent_ms = now_ms;
    c->stats_polls++;
    c->next_poll_ms = now_ms + (c->bus_fault ? VLX_CLIENT_FAULT_RETRY_MS : VLX_CLIENT_POLL_PERIOD_MS);
}

void vlx_client_tick(vlx_client_t *c, uint32_t now_ms)
{
    c->now_ms = now_ms;

    // 1. receive: the acknowledge byte is consumed; everything else goes to the parser
    uint8_t buf[32];
    size_t n;
    while ((n = hal_bus_read(buf, sizeof buf)) > 0) {
        size_t start = 0;
        if (c->write_state == VLX_WRITE_PENDING && !c->write_queued) {
            for (size_t i = 0; i < n; i++) {
                if (buf[i] == c->write_csum) {
                    c->write_state = VLX_WRITE_ACKED;
                    c->stats_acks++;
                    c->val[c->write_reg] = c->write_val;
                    c->ts[c->write_reg] = now_ms;
                    c->seen[c->write_reg] = true;
                    c->poll_next_override = c->write_reg;
                    c->heard_any = true;
                    c->last_heard_ms = now_ms;
                    if (i > 0) vlx_parser_feed_buffer(&c->parser, buf, i);
                    start = i + 1;
                    break;
                }
            }
        }
        if (start < n) vlx_parser_feed_buffer(&c->parser, buf + start, n - start);
    }

    // 2. timeouts
    if (c->poll_pending && (int32_t)(now_ms - (c->poll_sent_ms + VLX_CLIENT_RESPONSE_MS)) >= 0) {
        c->poll_pending = false;
        c->stats_timeouts++;
        if (c->unanswered < 255) c->unanswered++;
        if (c->unanswered >= VLX_CLIENT_FAULT_AFTER) c->bus_fault = true;
    }
    if (c->write_state == VLX_WRITE_PENDING && !c->write_queued &&
        (int32_t)(now_ms - (c->write_sent_ms + VLX_CLIENT_RESPONSE_MS)) >= 0) {
        if (c->write_tries < VLX_CLIENT_WRITE_TRIES) c->write_queued = true;
        else c->write_state = VLX_WRITE_FAILED;
    }

    if (!c->tx_enabled) return;

    // 3. send: a queued write first, else a poll when due and nothing is outstanding
    if (c->write_queued) {
        if (!c->poll_pending) send_write(c, now_ms);
        return;
    }
    if (c->write_state == VLX_WRITE_PENDING) return;
    if (c->poll_pending) return;
    if (c->poll_next_override) {
        uint8_t reg = c->poll_next_override;
        c->poll_next_override = 0;
        send_poll(c, reg, now_ms);
        return;
    }
    if (c->poll_n == 0) return;
    if ((int32_t)(now_ms - c->next_poll_ms) < 0) return;
    uint8_t reg = c->poll_list[c->poll_idx];
    c->poll_idx = (uint8_t)((c->poll_idx + 1) % c->poll_n);
    send_poll(c, reg, now_ms);
}

bool vlx_client_get(const vlx_client_t *c, uint8_t reg, uint8_t *val, uint32_t *age_ms, uint32_t now_ms)
{
    if (!c->seen[reg]) return false;
    if (val) *val = c->val[reg];
    if (age_ms) *age_ms = now_ms - c->ts[reg];
    return true;
}

bool vlx_client_is_stale(const vlx_client_t *c, uint8_t reg, uint32_t now_ms)
{
    return !c->seen[reg] || (now_ms - c->ts[reg]) >= VLX_CLIENT_STALE_MS;
}

vlx_write_result_t vlx_client_write(vlx_client_t *c, uint8_t reg, uint8_t val)
{
    if (!c->tx_enabled) return VLX_WRITE_REFUSED;
    if (!vlx_register_is_write_allowed(reg) || !vlx_value_is_valid_for(reg, val)) return VLX_WRITE_REFUSED;
    if (c->write_state == VLX_WRITE_PENDING || c->write_queued) return VLX_WRITE_BUSY;
    c->write_reg = reg;
    c->write_val = val;
    c->write_tries = 0;
    c->write_queued = true;
    c->write_state = VLX_WRITE_PENDING;
    return VLX_WRITE_QUEUED;
}

vlx_write_state_t vlx_client_write_state(const vlx_client_t *c) { return c->write_state; }

void vlx_client_write_clear(vlx_client_t *c)
{
    if (c->write_state == VLX_WRITE_ACKED || c->write_state == VLX_WRITE_FAILED) c->write_state = VLX_WRITE_IDLE;
}

bool vlx_client_bus_ok(const vlx_client_t *c, uint32_t now_ms)
{
    return c->heard_any && !c->bus_fault && (now_ms - c->last_heard_ms) < VLX_CLIENT_ALIVE_MS;
}

bool vlx_client_bus_fault(const vlx_client_t *c) { return c->bus_fault; }
