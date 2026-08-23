// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// panel_hal.h for the browser: the display lands in an RGBA8888 buffer JS
// copies to a canvas, the buttons are set from clicks and keys, the bus is a
// pair of byte rings between the UI core and the machine model inside the same
// module (every byte in either direction is also fed to the bus log), the
// clock is whatever sim_run() says, and the settings live in a RAM table JS
// mirrors to localStorage. Nothing here knows about three.js.
#include "hal_web.h"
#include <string.h>
#include "panel_hal.h"
#include "vallox_protocol.h"

#define RING 512
typedef struct { uint8_t buf[RING]; size_t head, tail; } ring_t;

static size_t ring_put(ring_t *r, const uint8_t *d, size_t n)
{
    size_t put = 0;
    while (put < n) {
        size_t next = (r->head + 1) % RING;
        if (next == r->tail) break;          // full: drop the rest, like a UART FIFO
        r->buf[r->head] = d[put++];
        r->head = next;
    }
    return put;
}
static size_t ring_get(ring_t *r, uint8_t *d, size_t max)
{
    size_t got = 0;
    while (got < max && r->tail != r->head) {
        d[got++] = r->buf[r->tail];
        r->tail = (r->tail + 1) % RING;
    }
    return got;
}

static ring_t   s_to_machine, s_to_panel;
static uint32_t s_now_ms;
static uint16_t s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
static const uint16_t k_ladder_mv[4] = { HAL_WEB_LADDER_SW1_MV, HAL_WEB_LADDER_SW2_MV,
                                         HAL_WEB_LADDER_SW3_MV, HAL_WEB_LADDER_SW4_MV };
static bool s_down[4], s_want_up[4];
static int  s_samples[4];          // hal_buttons_read_mv calls since the press
static uint8_t  s_rgba[HAL_DISPLAY_W * HAL_DISPLAY_H * 4];
static int      s_dirty_x0, s_dirty_y0, s_dirty_x1, s_dirty_y1;   // union, x1/y1 exclusive
static bool     s_dirty;
static int      s_flushes;
static uint8_t  s_leds;
static uint8_t  s_backlight;

typedef struct { char key[16]; uint8_t val[32]; size_t len; bool used; } slot_t;
static slot_t s_store[HAL_WEB_STORE_MAX];
static bool   s_store_dirty;

// bus log: two parsers, one per direction, so alignment is per stream
static uint8_t  s_log[HAL_WEB_LOG_MAX][HAL_WEB_LOG_ENTRY_BYTES];
static int      s_log_total;
static vlx_parser_t s_log_parser[2];

static void log_frame(const vlx_frame_t *f, void *ctx)
{
    uint8_t dir = (uint8_t)(uintptr_t)ctx;
    uint8_t *e = s_log[s_log_total % HAL_WEB_LOG_MAX];
    e[0] = (uint8_t)(s_now_ms); e[1] = (uint8_t)(s_now_ms >> 8);
    e[2] = (uint8_t)(s_now_ms >> 16); e[3] = (uint8_t)(s_now_ms >> 24);
    e[4] = dir;
    e[5] = f->domain; e[6] = f->sender; e[7] = f->receiver; e[8] = f->reg; e[9] = f->value; e[10] = f->checksum;
    e[11] = 0;
    s_log_total++;
}

void hal_web_reset(void)
{
    memset(&s_to_machine, 0, sizeof s_to_machine);
    memset(&s_to_panel, 0, sizeof s_to_panel);
    s_now_ms = 0;
    s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
    memset(s_down, 0, sizeof s_down); memset(s_want_up, 0, sizeof s_want_up); memset(s_samples, 0, sizeof s_samples);
    memset(s_rgba, 0, sizeof s_rgba);
    for (size_t i = 3; i < sizeof s_rgba; i += 4) s_rgba[i] = 255;
    s_dirty = false; s_flushes = 0;
    s_leds = 0; s_backlight = 0;
    // the store survives a reset on purpose: it is the device's NVS
    s_log_total = 0;
    vlx_parser_init(&s_log_parser[0], log_frame, (void *)(uintptr_t)0);
    vlx_parser_init(&s_log_parser[1], log_frame, (void *)(uintptr_t)1);
}

void hal_web_set_time_ms(uint32_t now_ms) { s_now_ms = now_ms; }


static void ladder_recompute(void)
{
    // the lowest-voltage pressed switch wins, as on the real ladder
    s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
    for (int i = 0; i < 4; i++) if (s_down[i]) { s_buttons_mv = k_ladder_mv[i]; break; }
}

// A click in a slow browser tab can be shorter than the core's debounce (two
// 20 ms samples), so a release is honoured only after HAL_WEB_MIN_HOLD_SAMPLES
// samples have seen the press. Holding longer is unaffected.
void hal_web_set_button(int idx, bool down)
{
    if (idx < 0 || idx > 3) return;
    if (down) { s_down[idx] = true; s_want_up[idx] = false; s_samples[idx] = 0; }
    else if (s_down[idx]) {
        if (s_samples[idx] >= HAL_WEB_MIN_HOLD_SAMPLES) s_down[idx] = false;
        else s_want_up[idx] = true;
    }
    ladder_recompute();
}
uint16_t hal_web_button_mv(void) { return s_buttons_mv; }

size_t hal_web_machine_read(uint8_t *buf, size_t max) { return ring_get(&s_to_machine, buf, max); }
size_t hal_web_machine_write(const uint8_t *buf, size_t len)
{
    vlx_parser_feed_buffer(&s_log_parser[1], buf, len);
    return ring_put(&s_to_panel, buf, len);
}

const uint8_t *hal_web_rgba(void) { return s_rgba; }
bool hal_web_take_dirty(int *x, int *y, int *w, int *h)
{
    if (!s_dirty) return false;
    *x = s_dirty_x0; *y = s_dirty_y0; *w = s_dirty_x1 - s_dirty_x0; *h = s_dirty_y1 - s_dirty_y0;
    s_dirty = false;
    return true;
}
int     hal_web_flushes(void)   { return s_flushes; }
uint8_t hal_web_leds(void)      { return s_leds; }
uint8_t hal_web_backlight(void) { return s_backlight; }

int hal_web_log_total(void) { return s_log_total; }
const uint8_t *hal_web_log_entry(int seq)
{
    if (seq < 0 || seq >= s_log_total || seq < s_log_total - HAL_WEB_LOG_MAX) return NULL;
    return s_log[seq % HAL_WEB_LOG_MAX];
}

// ---- panel_hal.h --------------------------------------------------------

void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)
{
    s_flushes++;
    for (uint16_t r = 0; r < h; r++) {
        if (y + r >= HAL_DISPLAY_H) break;
        uint8_t *dst = &s_rgba[((y + r) * HAL_DISPLAY_W + x) * 4];
        for (uint16_t c = 0; c < w; c++) {
            if (x + c >= HAL_DISPLAY_W) break;
            uint16_t p = rgb565[r * w + c];
            uint8_t r5 = (uint8_t)(p >> 11), g6 = (uint8_t)((p >> 5) & 0x3F), b5 = (uint8_t)(p & 0x1F);
            dst[0] = (uint8_t)((r5 << 3) | (r5 >> 2));
            dst[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
            dst[2] = (uint8_t)((b5 << 3) | (b5 >> 2));
            dst[3] = 255;
            dst += 4;
        }
    }
    int x1 = x + w > HAL_DISPLAY_W ? HAL_DISPLAY_W : x + w;
    int y1 = y + h > HAL_DISPLAY_H ? HAL_DISPLAY_H : y + h;
    if (!s_dirty) { s_dirty_x0 = x; s_dirty_y0 = y; s_dirty_x1 = x1; s_dirty_y1 = y1; s_dirty = true; }
    else {
        if (x < s_dirty_x0) s_dirty_x0 = x;
        if (y < s_dirty_y0) s_dirty_y0 = y;
        if (x1 > s_dirty_x1) s_dirty_x1 = x1;
        if (y1 > s_dirty_y1) s_dirty_y1 = y1;
    }
}

uint16_t hal_buttons_read_mv(void)
{
    uint16_t mv = s_buttons_mv;            // this sample still sees the press
    for (int i = 0; i < 4; i++) {
        if (!s_down[i]) continue;
        s_samples[i]++;
        if (s_want_up[i] && s_samples[i] >= HAL_WEB_MIN_HOLD_SAMPLES) { s_down[i] = false; s_want_up[i] = false; }
    }
    ladder_recompute();
    return mv;
}
void hal_leds_set(bool pwr, bool bus, bool fault) { s_leds = (uint8_t)((pwr ? 1 : 0) | (bus ? 2 : 0) | (fault ? 4 : 0)); }
void hal_backlight_set(uint8_t level) { s_backlight = level; }
size_t hal_bus_write(const uint8_t *buf, size_t len)
{
    vlx_parser_feed_buffer(&s_log_parser[0], buf, len);
    return ring_put(&s_to_machine, buf, len);
}
size_t hal_bus_read(uint8_t *buf, size_t max) { return ring_get(&s_to_panel, buf, max); }
uint32_t hal_time_ms(void) { return s_now_ms; }

static slot_t *find(const char *key)
{
    for (int i = 0; i < HAL_WEB_STORE_MAX; i++)
        if (s_store[i].used && strcmp(s_store[i].key, key) == 0) return &s_store[i];
    return NULL;
}
bool hal_store_get(const char *key, void *buf, size_t len)
{
    slot_t *s = find(key);
    if (!s || s->len != len) return false;
    memcpy(buf, s->val, len);
    return true;
}
bool hal_store_put(const char *key, const void *buf, size_t len)
{
    if (len > sizeof ((slot_t *)0)->val || strlen(key) >= sizeof ((slot_t *)0)->key) return false;
    slot_t *s = find(key);
    if (!s) {
        for (int i = 0; i < HAL_WEB_STORE_MAX && !s; i++)
            if (!s_store[i].used) s = &s_store[i];
        if (!s) return false;
        s->used = true;
        strcpy(s->key, key);
    }
    memcpy(s->val, buf, len);
    s->len = len;
    s_store_dirty = true;
    return true;
}

int hal_web_store_count(void)
{
    int n = 0;
    for (int i = 0; i < HAL_WEB_STORE_MAX; i++) if (s_store[i].used) n++;
    return n;
}
static slot_t *nth(int i)
{
    for (int k = 0; k < HAL_WEB_STORE_MAX; k++)
        if (s_store[k].used && i-- == 0) return &s_store[k];
    return NULL;
}
const char *hal_web_store_key(int i) { slot_t *s = nth(i); return s ? s->key : NULL; }
const uint8_t *hal_web_store_value(int i, int *len)
{
    slot_t *s = nth(i);
    if (!s) { *len = 0; return NULL; }
    *len = (int)s->len;
    return s->val;
}
bool hal_web_store_take_dirty(void) { bool d = s_store_dirty; s_store_dirty = false; return d; }
