// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "hal_host.h"
#include <string.h>
#include "panel_hal.h"

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

static ring_t s_to_machine, s_to_panel;
static uint32_t s_now_ms;
static uint16_t s_buttons_mv = 3300;
static uint16_t s_fb[HAL_DISPLAY_W * HAL_DISPLAY_H];
static bool s_led_pwr, s_led_bus, s_led_fault;
static uint8_t s_backlight;

#define STORE_MAX 16
typedef struct { char key[16]; uint8_t val[32]; size_t len; bool used; } slot_t;
static slot_t s_store[STORE_MAX];

void hal_host_reset(void)
{
    memset(&s_to_machine, 0, sizeof s_to_machine);
    memset(&s_to_panel, 0, sizeof s_to_panel);
    s_now_ms = 0;
    s_buttons_mv = 3300;
    memset(s_fb, 0, sizeof s_fb);
    s_led_pwr = s_led_bus = s_led_fault = false;
    s_backlight = 0;
    memset(s_store, 0, sizeof s_store);
}

void hal_host_advance_ms(uint32_t ms) { s_now_ms += ms; }
void hal_host_set_buttons_mv(uint16_t mv) { s_buttons_mv = mv; }

size_t membus_machine_read(uint8_t *buf, size_t max) { return ring_get(&s_to_machine, buf, max); }
size_t membus_machine_write(const uint8_t *buf, size_t len) { return ring_put(&s_to_panel, buf, len); }

const uint16_t *hal_host_framebuffer(void) { return s_fb; }
void hal_host_leds(bool *pwr, bool *bus, bool *fault) { *pwr = s_led_pwr; *bus = s_led_bus; *fault = s_led_fault; }
uint8_t hal_host_backlight(void) { return s_backlight; }

// ---- panel_hal.h --------------------------------------------------------

void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)
{
    for (uint16_t r = 0; r < h; r++) {
        if (y + r >= HAL_DISPLAY_H) break;
        for (uint16_t c = 0; c < w; c++) {
            if (x + c >= HAL_DISPLAY_W) break;
            s_fb[(y + r) * HAL_DISPLAY_W + (x + c)] = rgb565[r * w + c];
        }
    }
}

uint16_t hal_buttons_read_mv(void) { return s_buttons_mv; }
void hal_leds_set(bool pwr, bool bus, bool fault) { s_led_pwr = pwr; s_led_bus = bus; s_led_fault = fault; }
void hal_backlight_set(uint8_t level) { s_backlight = level; }
size_t hal_bus_write(const uint8_t *buf, size_t len) { return ring_put(&s_to_machine, buf, len); }
size_t hal_bus_read(uint8_t *buf, size_t max) { return ring_get(&s_to_panel, buf, max); }
uint32_t hal_time_ms(void) { return s_now_ms; }

static slot_t *find(const char *key)
{
    for (int i = 0; i < STORE_MAX; i++)
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
        for (int i = 0; i < STORE_MAX && !s; i++)
            if (!s_store[i].used) s = &s_store[i];
        if (!s) return false;
        s->used = true;
        strcpy(s->key, key);
    }
    memcpy(s->val, buf, len);
    s->len = len;
    return true;
}
