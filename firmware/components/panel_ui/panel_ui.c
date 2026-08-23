// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "panel_ui.h"
#include <string.h>
#include "buttons.h"
#include "gfx.h"
#include "icons.h"
#include "panel_hal.h"
#include "texts.h"
#include "theme.h"
#include "vallox_protocol.h"

// ---- state ------------------------------------------------------------------

typedef struct { page_id_t page; uint8_t sel; } frame_t;

static struct {
    frame_t  stack[PANEL_UI_STACK_MAX];
    int      depth;
    int16_t  edit_val;
    buttons_t btns;
    vlx_client_t client;
    uint32_t init_ms, last_button_ms, last_render_ms;
    bool     dimmed, need_render, reset_done, have_rendered;
    uint32_t splash_until; bool splash;
    int      shown_speed;
    const char *version;
} s;

static const page_t *cur(void) { return &pages[s.stack[s.depth - 1].page]; }
static frame_t *top(void) { return &s.stack[s.depth - 1]; }

// ---- small helpers (no stdio in the core) -------------------------------------

static char *fmt_int(char *out, int v)
{
    char tmp[12]; int n = 0; unsigned u = v < 0 ? (unsigned)(-v) : (unsigned)v;
    do { tmp[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    char *p = out;
    if (v < 0) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = '\0';
    return out;
}

// copy tpl to out (size n) replacing the first "%d" with v
static void fmt_tpl(char *out, size_t n, const char *tpl, int v)
{
    size_t o = 0;
    for (const char *p = tpl; *p && o + 1 < n; p++) {
        if (p[0] == '%' && p[1] == 'd') {
            char num[12]; fmt_int(num, v);
            for (const char *q = num; *q && o + 1 < n; q++) out[o++] = *q;
            p++;
        } else out[o++] = *p;
    }
    out[o] = '\0';
}

static void str_cat(char *dst, size_t n, const char *src)
{
    size_t d = strlen(dst);
    while (*src && d + 1 < n) dst[d++] = *src++;
    dst[d] = '\0';
}

// ---- shadow readers ------------------------------------------------------------

static bool reg_fresh(uint8_t reg, uint8_t *val, uint32_t now)
{
    return vlx_client_get(&s.client, reg, val, 0, now);
}
static bool reg_stale(uint8_t reg, uint32_t now) { return vlx_client_is_stale(&s.client, reg, now); }

static int shadow_speed(uint32_t now)
{
    uint8_t v;
    if (!reg_fresh(VLX_REG_FAN_SPEED, &v, now)) return -1;
    int sp = vlx_fan_speed_from_raw(v);
    return sp == VLX_FAN_SPEED_INVALID ? -1 : sp;
}

static bool fault_active(uint32_t now, uint8_t *code)
{
    uint8_t f = 0, st = 0;
    bool have_f = reg_fresh(VLX_REG_FAULT, &f, now);
    bool have_s = reg_fresh(VLX_REG_STATUS, &st, now);
    if (code) *code = f;
    return (have_f && f != 0) || (have_s && (st & VLX_STATUS_FAULT));
}

// Localised fault name for a 0x36 code; writes into buf and returns it.
static const char *fault_text(uint8_t code, char *buf, size_t n)
{
    text_key_t k;
    switch ((vlx_fault_t)code) {
    case VLX_FAULT_SUPPLY_AIR_SENSOR:  k = TXT_FAULT_SUPPLY_SENSOR; break;
    case VLX_FAULT_CO2_ALARM:          k = TXT_FAULT_CO2_ALARM; break;
    case VLX_FAULT_OUTDOOR_AIR_SENSOR: k = TXT_FAULT_OUTDOOR_SENSOR; break;
    case VLX_FAULT_EXTRACT_AIR_SENSOR: k = TXT_FAULT_EXTRACT_SENSOR; break;
    case VLX_FAULT_WATER_COIL_FROST:   k = TXT_FAULT_WATER_COIL_FROST; break;
    case VLX_FAULT_EXHAUST_AIR_SENSOR: k = TXT_FAULT_EXHAUST_SENSOR; break;
    default: fmt_tpl(buf, n, text_get(TXT_FAULT_UNKNOWN), code); return buf;
    }
    buf[0] = '\0';
    str_cat(buf, n, text_get(k));
    return buf;
}

// ---- navigation ------------------------------------------------------------------

static void push(page_id_t p)
{
    if (s.depth >= PANEL_UI_STACK_MAX) return;
    s.stack[s.depth].page = p; s.stack[s.depth].sel = 0; s.depth++;
    if (pages[p].kind == PAGE_KIND_EDITOR) {
        const page_t *pg = &pages[p];
        uint32_t now = hal_time_ms();
        uint8_t raw;
        s.edit_val = pg->min;
        if (pg->enc == ENC_FAN_SPEED) { int sp = shadow_speed(now); if (sp > 0) s.edit_val = (int16_t)sp; }
        else if (pg->enc == ENC_TEMP_C) { if (reg_fresh(pg->reg, &raw, now)) s.edit_val = vlx_temp_table(raw); }
        else if (pg->enc == ENC_LANG) s.edit_val = (int16_t)text_lang();
        if (s.edit_val < pg->min) s.edit_val = pg->min;
        if (s.edit_val > pg->max) s.edit_val = pg->max;
    }
    s.need_render = true;
}

static void pop(void) { if (s.depth > 1) { s.depth--; s.need_render = true; } }
static void home(void) { s.depth = 1; s.need_render = true; }

static bool editor_writable(const page_t *pg)
{
    if (pg->enc == ENC_LANG) return true;
    return vlx_register_is_write_allowed(pg->reg);
}

static void editor_save(const page_t *pg)
{
    if (pg->enc == ENC_LANG) {
        uint8_t l = (uint8_t)s.edit_val;
        text_set_lang((lang_t)l);
        hal_store_put("lang", &l, 1);
    } else if (pg->enc == ENC_FAN_SPEED && editor_writable(pg)) {
        vlx_client_write_clear(&s.client);
        vlx_client_write(&s.client, pg->reg, vlx_fan_speed_to_raw(s.edit_val));
    } else if (pg->enc == ENC_TEMP_C && editor_writable(pg)) {
        vlx_client_write_clear(&s.client);
        vlx_client_write(&s.client, pg->reg, vlx_temp_to_raw(s.edit_val));
    }
    pop();
}

static void dashboard_adjust(int delta, uint32_t now)
{
    int sp = shadow_speed(now);
    if (sp < 0) return;
    sp += delta;
    if (sp < 1) sp = 1;
    if (sp > 8) sp = 8;
    vlx_client_write_clear(&s.client);
    vlx_client_write(&s.client, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(sp));
}

static void handle_event(button_event_t ev, uint32_t now)
{
    const page_t *pg = cur();
    if (ev.kind == BEV_LONG) { if (ev.button == BTN_BACK) home(); return; }
    switch (pg->kind) {
    case PAGE_KIND_DASHBOARD:
        if (ev.kind != BEV_PRESS) return;
        if (ev.button == BTN_OK) push(PAGE_MENU);
        else if (ev.button == BTN_PLUS) dashboard_adjust(+1, now);
        else if (ev.button == BTN_MINUS) dashboard_adjust(-1, now);
        break;
    case PAGE_KIND_LIST:
        if (ev.kind != BEV_PRESS && ev.kind != BEV_REPEAT) return;
        if (ev.button == BTN_PLUS && top()->sel + 1 < pg->n) top()->sel++;
        else if (ev.button == BTN_MINUS && top()->sel > 0) top()->sel--;
        else if (ev.button == BTN_OK && ev.kind == BEV_PRESS) push(pg->items[top()->sel].target);
        else if (ev.button == BTN_BACK && ev.kind == BEV_PRESS) pop();
        break;
    case PAGE_KIND_EDITOR:
        if (ev.button == BTN_PLUS || ev.button == BTN_MINUS) {
            if (!editor_writable(pg)) return;
            int v = s.edit_val + (ev.button == BTN_PLUS ? pg->step : -pg->step);
            if (v < pg->min) v = pg->min;
            if (v > pg->max) v = pg->max;
            s.edit_val = (int16_t)v;
        } else if (ev.kind == BEV_PRESS && ev.button == BTN_OK) {
            if (editor_writable(pg)) editor_save(pg); else pop();
        } else if (ev.kind == BEV_PRESS && ev.button == BTN_BACK) pop();
        break;
    case PAGE_KIND_INFO:
        if (ev.kind == BEV_PRESS && (ev.button == BTN_BACK || ev.button == BTN_OK)) pop();
        break;
    }
    s.need_render = true;
}

// ---- drawing ---------------------------------------------------------------------

static void draw_top_bar(uint32_t now, const char *title)
{
    gfx_fill_rect(0, 0, HAL_DISPLAY_W, THEME_TOP_H, THEME_BAR);
    gfx_text(THEME_MARGIN, 1, &font_inter_18, THEME_FG, title);
    bool ok = vlx_client_bus_ok(&s.client, now);
    gfx_icon(HAL_DISPLAY_W - THEME_MARGIN - 12, 6, &icon_bus, ok ? THEME_ACCENT : THEME_FAULT);
    if (!ok) gfx_text_right(HAL_DISPLAY_W - THEME_MARGIN - 16, 5, &font_inter_12, THEME_FAULT, text_get(TXT_NO_BUS));
    if (fault_active(now, 0)) gfx_icon(HAL_DISPLAY_W - THEME_MARGIN - 12 - 20, 6, &icon_fault, THEME_FAULT);
}

static void draw_bottom_bar(const char *l0, const char *l1, const char *l2, const char *l3)
{
    gfx_fill_rect(0, THEME_BOTTOM_Y, HAL_DISPLAY_W, THEME_BOTTOM_H, THEME_BAR);
    if (s.dimmed) return;                       // nothing to press until woken
    int y = THEME_BOTTOM_Y + 5;
    if (l0) gfx_text_centred(THEME_BTN_X0, y, &font_inter_18, THEME_FG_DIM, l0);
    if (l1) gfx_text_centred(THEME_BTN_X1, y, &font_inter_18, THEME_FG_DIM, l1);
    if (l2) gfx_text_centred(THEME_BTN_X2, y, &font_inter_18, THEME_FG_DIM, l2);
    if (l3) gfx_text_centred(THEME_BTN_X3, y, &font_inter_18, THEME_FG_DIM, l3);
}

static void clear_content(void)
{
    gfx_fill_rect(0, THEME_CONTENT_Y, HAL_DISPLAY_W, THEME_CONTENT_H, THEME_BG);
}

static void draw_temp_row(int x_label, int x_right, int y, text_key_t label, uint8_t reg, uint32_t now)
{
    uint8_t raw; char buf[16];
    gfx_text(x_label, y + 3, &font_inter_12, THEME_FG_DIM, text_get(label));
    if (reg_fresh(reg, &raw, now) && !vlx_temp_is_saturated(raw)) {
        fmt_int(buf, vlx_temp_table(raw));
        str_cat(buf, sizeof buf, text_get(TXT_UNIT_C));
        gfx_text_right(x_right, y, &font_inter_18, reg_stale(reg, now) ? THEME_FG_DIM : THEME_FG, buf);
    } else {
        gfx_text_right(x_right, y, &font_inter_18, THEME_FG_DIM, text_get(TXT_STALE));
    }
}

static void draw_dashboard(uint32_t now)
{
    char buf[48];
    draw_top_bar(now, text_get(TXT_DASHBOARD));
    clear_content();
    // fan speed, large, left
    int sp = shadow_speed(now);
    bool stale = reg_stale(VLX_REG_FAN_SPEED, now);
    s.shown_speed = (sp > 0 && !stale) ? sp : -1;
    gfx_text(THEME_MARGIN, THEME_CONTENT_Y + 6, &font_inter_12, THEME_FG_DIM, text_get(TXT_FAN_SPEED));
    if (sp > 0) {
        fmt_int(buf, sp);
        gfx_text(THEME_MARGIN, THEME_CONTENT_Y + 22, &font_inter_36, stale ? THEME_FG_DIM : THEME_FG, buf);
        int w = font_text_width(&font_inter_36, buf);
        gfx_text(THEME_MARGIN + w + 6, THEME_CONTENT_Y + 22 + 18, &font_inter_12, THEME_FG_DIM, text_get(TXT_SPEED_OF));
    } else {
        gfx_text(THEME_MARGIN, THEME_CONTENT_Y + 22, &font_inter_36, THEME_FG_DIM, text_get(TXT_STALE));
    }
    // eight-segment bar
    for (int i = 0; i < 8; i++)
        gfx_fill_rect(THEME_MARGIN + i * 17, THEME_CONTENT_Y + 74, 14, 8, (sp > i && !stale) ? THEME_ACCENT : THEME_SELECT);
    // four temperatures, right column
    const int xl = 170, xr = HAL_DISPLAY_W - THEME_MARGIN;
    draw_temp_row(xl, xr, THEME_CONTENT_Y + 6,  TXT_OUTDOOR, VLX_REG_TEMP_OUTDOOR, now);
    draw_temp_row(xl, xr, THEME_CONTENT_Y + 38, TXT_SUPPLY,  VLX_REG_TEMP_SUPPLY, now);
    draw_temp_row(xl, xr, THEME_CONTENT_Y + 70, TXT_EXTRACT, VLX_REG_TEMP_EXTRACT, now);
    draw_temp_row(xl, xr, THEME_CONTENT_Y + 102, TXT_EXHAUST, VLX_REG_TEMP_EXHAUST, now);
    // status line: heating / bypass, boost
    uint8_t st, bm;
    int y = THEME_CONTENT_Y + 136;
    int x = THEME_MARGIN;
    if (reg_fresh(VLX_REG_STATUS, &st, now)) {
        if (st & VLX_STATUS_HEATING) {
            gfx_icon(x, y, &icon_heater, THEME_WARN);
            gfx_text(x + 16, y - 1, &font_inter_12, THEME_FG, text_get(TXT_HEATER_ON));
            x += 16 + font_text_width(&font_inter_12, text_get(TXT_HEATER_ON)) + 16;
        } else if (!(st & VLX_STATUS_WINTER_MODE)) {
            gfx_icon(x, y, &icon_bypass, THEME_ACCENT);
            gfx_text(x + 16, y - 1, &font_inter_12, THEME_FG, text_get(TXT_BYPASS));
            x += 16 + font_text_width(&font_inter_12, text_get(TXT_BYPASS)) + 16;
        }
    }
    if (reg_fresh(VLX_REG_BOOST_MINUTES, &bm, now) && bm > 0) {
        gfx_icon(x, y, &icon_boost, THEME_ACCENT);
        fmt_tpl(buf, sizeof buf, text_get(TXT_BOOST_MIN), bm);
        gfx_text(x + 16, y - 1, &font_inter_12, THEME_FG, buf);
    }
    // fault banner
    uint8_t code;
    if (fault_active(now, &code)) {
        gfx_round_rect(THEME_MARGIN, THEME_CONTENT_Y + 154, HAL_DISPLAY_W - 2 * THEME_MARGIN, 26, 4, THEME_FAULT);
        char name[40];
        buf[0] = '\0';
        str_cat(buf, sizeof buf, text_get(TXT_FAULT));
        str_cat(buf, sizeof buf, ": ");
        str_cat(buf, sizeof buf, fault_text(code, name, sizeof name));
        gfx_text(THEME_MARGIN + 8, THEME_CONTENT_Y + 156, &font_inter_18, THEME_FG, buf);
    }
    draw_bottom_bar(text_get(TXT_BTN_MINUS), text_get(TXT_BTN_PLUS), text_get(TXT_MENU), 0);
}

static void draw_list(uint32_t now, const page_t *pg)
{
    draw_top_bar(now, text_get(pg->title));
    clear_content();
    for (int i = 0; i < pg->n; i++) {
        int y = THEME_CONTENT_Y + 4 + i * THEME_ROW_H;
        if (i == top()->sel) {
            gfx_fill_rect(0, y, HAL_DISPLAY_W, THEME_ROW_H, THEME_SELECT);
            gfx_fill_rect(0, y, 4, THEME_ROW_H, THEME_ACCENT);
        }
        gfx_text(16, y + 3, &font_inter_18, THEME_FG, text_get(pg->items[i].label));
        gfx_text_right(HAL_DISPLAY_W - THEME_MARGIN - 4, y + 3, &font_inter_18, THEME_FG_DIM, ">");
    }
    draw_bottom_bar(text_get(TXT_BTN_UP), text_get(TXT_BTN_DOWN), text_get(TXT_BTN_OK), text_get(TXT_BTN_BACK));
}

static void draw_editor(uint32_t now, const page_t *pg)
{
    char buf[16];
    draw_top_bar(now, text_get(pg->title));
    clear_content();
    bool writable = editor_writable(pg);
    int cy = THEME_CONTENT_Y + 40;
    if (pg->enc == ENC_LANG) {
        gfx_text_centred(HAL_DISPLAY_W / 2, cy + 8, &font_inter_18, THEME_FG,
                         text_get(s.edit_val == LANG_FI ? TXT_FINNISH : TXT_ENGLISH));
    } else {
        fmt_int(buf, s.edit_val);
        gfx_text_centred(HAL_DISPLAY_W / 2, cy, &font_inter_36, writable ? THEME_FG : THEME_FG_DIM, buf);
        gfx_text_centred(HAL_DISPLAY_W / 2, cy + 50, &font_inter_18, THEME_FG_DIM,
                         text_get(pg->enc == ENC_FAN_SPEED ? TXT_SPEED_OF : TXT_UNIT_C));
    }
    if (!writable) gfx_text_centred(HAL_DISPLAY_W / 2, cy + 90, &font_inter_12, THEME_WARN, text_get(TXT_READ_ONLY));
    if (writable) draw_bottom_bar(text_get(TXT_BTN_MINUS), text_get(TXT_BTN_PLUS), text_get(TXT_BTN_SAVE), text_get(TXT_BTN_BACK));
    else          draw_bottom_bar(0, 0, text_get(TXT_BTN_OK), text_get(TXT_BTN_BACK));
}

static void draw_status(uint32_t now, const page_t *pg)
{
    char buf[64];
    uint8_t code, months;
    draw_top_bar(now, text_get(pg->title));
    clear_content();
    int y = THEME_CONTENT_Y + 8;
    // fault
    buf[0] = '\0';
    if (fault_active(now, &code)) { char name[40]; str_cat(buf, sizeof buf, text_get(TXT_FAULT)); str_cat(buf, sizeof buf, ": "); str_cat(buf, sizeof buf, fault_text(code, name, sizeof name)); }
    else str_cat(buf, sizeof buf, text_get(TXT_NO_FAULT));
    gfx_text(16, y, &font_inter_18, THEME_FG, buf); y += THEME_ROW_H;
    // service
    if (reg_fresh(VLX_REG_SERVICE_MONTHS_LEFT, &months, now)) {
        if (months == 0) { buf[0] = '\0'; str_cat(buf, sizeof buf, text_get(TXT_SERVICE_NOW)); }
        else fmt_tpl(buf, sizeof buf, text_get(TXT_SERVICE_IN), months);
    } else { buf[0] = '\0'; str_cat(buf, sizeof buf, text_get(TXT_STALE)); }
    gfx_text(16, y, &font_inter_18, THEME_FG, buf); y += THEME_ROW_H;
    // bus
    buf[0] = '\0';
    str_cat(buf, sizeof buf, text_get(TXT_BUS)); str_cat(buf, sizeof buf, ": ");
    str_cat(buf, sizeof buf, vlx_client_bus_ok(&s.client, now) ? text_get(TXT_OK_WORD) : text_get(TXT_NO_BUS));
    gfx_text(16, y, &font_inter_18, THEME_FG, buf); y += THEME_ROW_H;
    // firmware
    buf[0] = '\0';
    str_cat(buf, sizeof buf, text_get(TXT_FIRMWARE)); str_cat(buf, sizeof buf, ": "); str_cat(buf, sizeof buf, s.version);
    gfx_text(16, y, &font_inter_18, THEME_FG, buf);
    draw_bottom_bar(0, 0, text_get(TXT_BTN_OK), text_get(TXT_BTN_BACK));
}

static void draw_splash(uint32_t now)
{
    draw_top_bar(now, text_get(TXT_SETTINGS));
    clear_content();
    gfx_text_centred(HAL_DISPLAY_W / 2, THEME_CONTENT_Y + 70, &font_inter_18, THEME_FG, text_get(TXT_FACTORY_RESET));
    draw_bottom_bar(0, 0, 0, 0);
}

static void render(uint32_t now)
{
    if (s.splash) { draw_splash(now); }
    else {
        const page_t *pg = cur();
        switch (pg->kind) {
        case PAGE_KIND_DASHBOARD: draw_dashboard(now); break;
        case PAGE_KIND_LIST:      draw_list(now, pg); break;
        case PAGE_KIND_EDITOR:    draw_editor(now, pg); break;
        case PAGE_KIND_INFO:      draw_status(now, pg); break;
        }
    }
    gfx_flush();
    s.last_render_ms = now;
    s.need_render = false;
    s.have_rendered = true;
}

// ---- public ----------------------------------------------------------------------

void panel_ui_init(void)
{
    const char *keep_version = s.version;
    memset(&s, 0, sizeof s);
    s.version = keep_version ? keep_version : "dev";
    s.shown_speed = -1;
    s.init_ms = hal_time_ms();
    s.last_button_ms = s.init_ms;
    s.depth = 1; s.stack[0].page = PAGE_DASHBOARD; s.stack[0].sel = 0;
    uint8_t lang = 0;
    if (hal_store_get("lang", &lang, 1)) text_set_lang((lang_t)lang); else text_set_lang(LANG_EN);
    buttons_init(&s.btns);
    vlx_client_init(&s.client, VLX_ADDR_PANEL_DEFAULT);
    gfx_init();
    hal_backlight_set(255);
    hal_leds_set(true, false, false);
    s.need_render = true;
}

void panel_ui_tick(uint32_t now)
{
    button_event_t ev = buttons_tick(&s.btns, hal_buttons_read_mv(), now);

    // factory reset: ← held from power-up for RESET_HOLD_MS
    if (!s.reset_done && buttons_held(&s.btns) == BTN_BACK &&
        (int32_t)(s.btns.press_ms - s.init_ms) <= (int32_t)(2 * PANEL_UI_TICK_MS) &&
        buttons_held_ms(&s.btns, now) >= PANEL_UI_RESET_HOLD_MS) {
        uint8_t en = 0;
        hal_store_put("lang", &en, 1);
        text_set_lang(LANG_EN);
        s.reset_done = true;
        s.splash = true; s.splash_until = now + PANEL_UI_SPLASH_MS;
        s.need_render = true;
        ev.kind = BEV_NONE;
    }
    if (s.splash && (int32_t)(now - s.splash_until) >= 0) { s.splash = false; s.need_render = true; }

    vlx_client_tick(&s.client, now);

    if (ev.kind != BEV_NONE) {
        s.last_button_ms = now;
        if (s.dimmed) {                     // wake, consume
            s.dimmed = false;
            hal_backlight_set(255);
            s.need_render = true;
        } else if (!s.splash) {
            handle_event(ev, now);
        }
    }
    // held buttons (repeats) also count as activity
    if (buttons_held(&s.btns) != BTN_NONE) s.last_button_ms = now;

    if (s.depth > 1 && (now - s.last_button_ms) >= PANEL_UI_HOME_MS) home();
    if (!s.dimmed && (now - s.last_button_ms) >= PANEL_UI_DIM_MS) {
        s.dimmed = true;
        hal_backlight_set(PANEL_UI_DIM_LEVEL);
        s.need_render = true;
    }

    bool fault = fault_active(now, 0);
    hal_leds_set(true, vlx_client_bus_ok(&s.client, now), fault);

    if (s.need_render || !s.have_rendered || (now - s.last_render_ms) >= PANEL_UI_RENDER_MS) render(now);
}

void panel_ui_set_version(const char *v) { s.version = v ? v : "dev"; }

vlx_client_t *panel_ui_client(void) { return &s.client; }
page_id_t panel_ui_current_page(void) { return s.stack[s.depth - 1].page; }
int panel_ui_page_depth(void) { return s.depth; }
int panel_ui_list_selection(void) { return cur()->kind == PAGE_KIND_LIST ? top()->sel : -1; }
int panel_ui_editor_value(void) { return cur()->kind == PAGE_KIND_EDITOR ? s.edit_val : INT16_MIN; }
bool panel_ui_is_dimmed(void) { return s.dimmed; }
bool panel_ui_splash_active(void) { return s.splash; }
int panel_ui_dashboard_speed(void) { return s.shown_speed; }
