// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Host runner: the UI core and the simulated machine over the memory bus,
// driven from the command line, writing the display as a PNG on demand.
//   panel_host [--lang en|fi] [--fault N] [--outdoor C] [--ticks N]
//              [--press -,+,ok,back,long-back,wait:MS,...] [--png out.png]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"
#include "panel_ui.h"
#include "texts.h"
#include "png.h"

static vlx_machine_t s_m;

static void tick(void)
{
    uint8_t buf[64];
    size_t n = membus_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    hal_host_advance_ms(PANEL_UI_TICK_MS);
    n = vlx_machine_tick(&s_m, hal_time_ms(), buf, sizeof buf);
    if (n) membus_machine_write(buf, n);
    panel_ui_tick(hal_time_ms());
}
static void run_ms(uint32_t ms) { for (uint32_t t = 0; t < ms; t += PANEL_UI_TICK_MS) tick(); }
static void press_mv(uint16_t mv, uint32_t hold) { hal_host_set_buttons_mv(mv); run_ms(hold); hal_host_set_buttons_mv(3300); run_ms(300); }

static void run_presses(const char *seq)
{
    char buf[256]; strncpy(buf, seq, sizeof buf - 1); buf[sizeof buf - 1] = '\0';
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        if (!strcmp(tok, "-")) press_mv(0, 60);
        else if (!strcmp(tok, "+")) press_mv(430, 60);
        else if (!strcmp(tok, "ok")) press_mv(819, 60);
        else if (!strcmp(tok, "back")) press_mv(1336, 60);
        else if (!strcmp(tok, "long-back")) press_mv(1336, 1200);
        else if (!strncmp(tok, "wait:", 5)) run_ms((uint32_t)atoi(tok + 5));
        else fprintf(stderr, "unknown press token: %s\n", tok);
    }
}

int main(int argc, char **argv)
{
    const char *lang = "en", *png = NULL, *presses = NULL;
    int fault = 0, ticks = 150; float outdoor = 5.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
        else if (!strcmp(argv[i], "--fault") && i + 1 < argc) fault = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--outdoor") && i + 1 < argc) outdoor = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--press") && i + 1 < argc) presses = argv[++i];
        else if (!strcmp(argv[i], "--png") && i + 1 < argc) png = argv[++i];
        else { fprintf(stderr, "usage: panel_host [--lang en|fi] [--fault N] [--outdoor C] [--ticks N] [--press seq] [--png out.png]\n"); return 2; }
    }
    hal_host_reset();
    vlx_machine_init(&s_m);
    s_m.p.t_outdoor = outdoor;
    uint8_t l = (uint8_t)(!strcmp(lang, "fi") ? 1 : 0);
    hal_store_put("lang", &l, 1);
    panel_ui_set_version("host");
    panel_ui_init();
    if (fault) vlx_machine_fault(&s_m, (vlx_fault_t)fault);
    for (int i = 0; i < ticks; i++) tick();
    if (presses) run_presses(presses);
    bool p, b, f; hal_host_leds(&p, &b, &f);
    printf("page=%d depth=%d speed=%d bus=%s fault_led=%d backlight=%u t=%u ms\n",
           (int)panel_ui_current_page(), panel_ui_page_depth(), panel_ui_dashboard_speed(),
           vlx_client_bus_ok(panel_ui_client(), hal_time_ms()) ? "ok" : "no", (int)f,
           hal_host_backlight(), (unsigned)hal_time_ms());
    if (png) {
        if (!png_write_rgb565(png, hal_host_framebuffer(), HAL_DISPLAY_W, HAL_DISPLAY_H)) { fprintf(stderr, "cannot write %s\n", png); return 1; }
        printf("wrote %s\n", png);
    }
    return 0;
}
