// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The simulator: the UI core and the machine model over the browser HAL's
// memory bus, advanced in PANEL_UI_TICK_MS steps by sim_run(). This is the
// same loop as firmware/test/host/panel_host.c, exported to JavaScript.
#include "sim_api.h"
#include <string.h>
#include "hal_web.h"
#include "panel_hal.h"
#include "panel_ui.h"
#include "texts.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#ifndef SIM_VERSION
#define SIM_VERSION "dev"
#endif

static vlx_machine_t s_m;
static uint32_t      s_now_ms;
static uint32_t      s_carry_ms;      // sub-tick remainder between sim_run calls

static void tick_once(void)
{
    uint8_t buf[64];
    size_t n = hal_web_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    s_now_ms += PANEL_UI_TICK_MS;
    hal_web_set_time_ms(s_now_ms);
    n = vlx_machine_tick(&s_m, s_now_ms, buf, sizeof buf);
    if (n) hal_web_machine_write(buf, n);
    panel_ui_tick(s_now_ms);
}

void sim_init(void)
{
    hal_web_reset();
    s_now_ms = 0; s_carry_ms = 0;
    vlx_machine_init(&s_m);
    panel_ui_set_version(SIM_VERSION);
    panel_ui_init();
}

void sim_run(uint32_t elapsed_ms)
{
    if (elapsed_ms > SIM_MAX_STEP_MS) elapsed_ms = SIM_MAX_STEP_MS;
    s_carry_ms += elapsed_ms;
    while (s_carry_ms >= PANEL_UI_TICK_MS) { tick_once(); s_carry_ms -= PANEL_UI_TICK_MS; }
}
uint32_t    sim_time_ms(void) { return s_now_ms; }
const char *sim_version(void) { return SIM_VERSION; }

const uint8_t *sim_fb_rgba(void) { return hal_web_rgba(); }
int sim_fb_take_dirty(int *x, int *y, int *w, int *h) { return hal_web_take_dirty(x, y, w, h) ? 1 : 0; }
int sim_fb_flushes(void) { return hal_web_flushes(); }
int sim_backlight(void)  { return hal_web_backlight(); }
int sim_leds(void)       { return hal_web_leds(); }

void sim_button(int idx, int down) { hal_web_set_button(idx, down != 0); }
int  sim_button_mv(void) { return hal_web_button_mv(); }

void sim_machine_set_outdoor(float c)     { s_m.p.t_outdoor = c; }
void sim_machine_set_time_scale(float s)  { if (s > 0.0f) s_m.p.time_scale = s; }
void sim_machine_set_reply_delay(int ms)  { s_m.reply_delay_ms = ms < 0 ? VLX_MACHINE_NEVER : (uint16_t)ms; }
void sim_machine_fault(int code)          { vlx_machine_fault(&s_m, (vlx_fault_t)code); }
void sim_machine_fault_clear(void)        { vlx_machine_fault_clear(&s_m); }
float sim_machine_temp(int which)
{
    switch (which) {
    case 0: return s_m.p.t_outdoor;
    case 1: return s_m.t_supply;
    case 2: return s_m.t_extract;
    case 3: return s_m.t_exhaust;
    default: return 0.0f;
    }
}
int sim_machine_reg(int reg)
{
    if (reg < 0 || reg > 255 || !vlx_machine_reg_known(&s_m, (uint8_t)reg)) return -1;
    return vlx_machine_reg_get(&s_m, (uint8_t)reg);
}
int sim_machine_fan_speed(void) { return vlx_fan_speed_from_raw(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED)); }
int sim_machine_flags(void)
{
    uint8_t st = vlx_machine_reg_get(&s_m, VLX_REG_STATUS);
    uint8_t io = vlx_machine_reg_get(&s_m, VLX_REG_IO_MULTI_2);
    int f = 0;
    if (st & VLX_STATUS_HEATING)       f |= 1;
    if (!(st & VLX_STATUS_WINTER_MODE)) f |= 2;
    if (io & VLX_IO2_SUPPLY_FAN_OFF)   f |= 4;
    if (st & VLX_STATUS_FAULT)         f |= 8;
    return f;
}

int  sim_ui_page(void)   { return (int)panel_ui_current_page(); }
int  sim_ui_depth(void)  { return panel_ui_page_depth(); }
int  sim_ui_dimmed(void) { return panel_ui_is_dimmed() ? 1 : 0; }
int  sim_ui_bus_ok(void) { return vlx_client_bus_ok(panel_ui_client(), s_now_ms) ? 1 : 0; }
int  sim_ui_lang(void)   { return (int)text_lang(); }
void sim_ui_set_lang(int lang)
{
    uint8_t l = (uint8_t)(lang == 1 ? 1 : 0);
    hal_store_put("lang", &l, 1);
    text_set_lang((lang_t)l);
}

int sim_log_total(void) { return hal_web_log_total(); }
const uint8_t *sim_log_entry(int seq) { return hal_web_log_entry(seq); }
const char *sim_reg_name(int reg)   { return (reg < 0 || reg > 255) ? "" : vlx_register_name((uint8_t)reg); }
const char *sim_fault_name(int code) { return (code < 0 || code > 255) ? "" : vlx_fault_name((uint8_t)code); }

int sim_store_count(void) { return hal_web_store_count(); }
const char *sim_store_key(int i) { return hal_web_store_key(i); }
const uint8_t *sim_store_value(int i, int *len) { return hal_web_store_value(i, len); }
int sim_store_put(const char *key, const uint8_t *val, int len)
{
    if (len < 0) return 0;
    bool ok = hal_store_put(key, val, (size_t)len);
    hal_web_store_take_dirty();      // a JS-originated put is not news to JS
    return ok ? 1 : 0;
}
int sim_store_take_dirty(void) { return hal_web_store_take_dirty() ? 1 : 0; }
