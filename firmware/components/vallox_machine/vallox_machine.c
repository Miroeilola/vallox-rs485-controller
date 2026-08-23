// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "vallox_machine.h"
#include <string.h>
#include "vallox_machine_regs.h"

#define ME VLX_ADDR_MAINBOARD_1

static void queue_bytes(vlx_machine_t *m, const uint8_t *b, size_t n)
{
    if (m->pending_len + n > sizeof m->pending) return;   // drop: the real bus has no queue either
    memcpy(m->pending + m->pending_len, b, n);
    m->pending_len += n;
    m->pending_armed = true;
}

static void answer_poll(vlx_machine_t *m, const vlx_frame_t *f)
{
    uint8_t reg = f->value;                 // a poll carries the wanted register in value
    if (!m->known[reg]) return;             // unknown: silence, like the documents say nothing
    if (m->reply_delay_ms == VLX_MACHINE_NEVER) return;
    uint8_t out[VLX_FRAME_LEN];
    vlx_make_write(ME, f->sender, reg, m->regs[reg], out);   // an answer has the same shape as a write
    queue_bytes(m, out, VLX_FRAME_LEN);
}

static void on_frame(const vlx_frame_t *f, void *ctx)
{
    vlx_machine_t *m = (vlx_machine_t *)ctx;
    if (f->sender == ME) return;                                   // our own echo
    if (f->receiver != ME && f->receiver != VLX_ADDR_MAINBOARDS) return;
    if (vlx_frame_is_poll(f)) { answer_poll(m, f); return; }
    // writes: Task 3
}

void vlx_machine_init(vlx_machine_t *m)
{
    memset(m, 0, sizeof *m);
    for (size_t i = 0; i < sizeof k_regs / sizeof k_regs[0]; i++) {
        const vlx_machine_reg_t *r = &k_regs[i];
        m->known[r->reg] = true; m->writable[r->reg] = r->writable;
        m->regs[r->reg] = r->def; m->conf[r->reg] = r->conf;
    }
    for (size_t i = 0; i < sizeof k_temp_regs / sizeof k_temp_regs[0]; i++) {
        const temp_reg_def_t *r = &k_temp_regs[i];
        m->known[r->reg] = true; m->writable[r->reg] = r->writable;
        m->regs[r->reg] = vlx_temp_to_raw(r->def_c); m->conf[r->reg] = r->conf;
    }
    m->reply_delay_ms = 0;
    m->broadcast_period_ms = 12000;
    m->broadcast_spacing_ms = 130;
    m->p.t_outdoor = 5.0f; m->p.t_indoor = 21.0f; m->p.efficiency = 0.6f;
    m->p.tau_s = 600.0f; m->p.time_scale = 1.0f;
    m->t_extract = 21.0f; m->t_supply = 17.0f; m->t_exhaust = 9.0f;
    vlx_parser_init(&m->parser, on_frame, m);
}

void vlx_machine_feed(vlx_machine_t *m, const uint8_t *bytes, size_t n)
{
    vlx_parser_feed_buffer(&m->parser, bytes, n);
}

size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max)
{
    m->last_tick_ms = now_ms;
    m->have_tick = true;
    if (m->pending_armed && !m->pending_scheduled) {
        m->pending_due_ms = now_ms + m->reply_delay_ms;
        m->pending_scheduled = true;
    }
    size_t n = 0;
    if (m->pending_scheduled && (int32_t)(now_ms - m->pending_due_ms) >= 0 && m->pending_len <= max) {
        memcpy(out, m->pending, m->pending_len);
        n = m->pending_len;
        m->pending_len = 0;
        m->pending_armed = false;
        m->pending_scheduled = false;
    }
    return n;
}

bool    vlx_machine_reg_known(const vlx_machine_t *m, uint8_t reg) { return m->known[reg]; }
uint8_t vlx_machine_reg_get(const vlx_machine_t *m, uint8_t reg)   { return m->regs[reg]; }
void    vlx_machine_reg_set(vlx_machine_t *m, uint8_t reg, uint8_t value)
{
    m->regs[reg] = value;
    m->known[reg] = true;
}

void vlx_machine_fault(vlx_machine_t *m, vlx_fault_t code) { (void)m; (void)code; }   // Task 6
void vlx_machine_fault_clear(vlx_machine_t *m) { (void)m; }                           // Task 6
