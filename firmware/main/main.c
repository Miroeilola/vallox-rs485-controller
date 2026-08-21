// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Vallox RS-485 Controller — application entry point.
//
// The application layer owns transports (UART, Wi-Fi, MQTT) and lifecycle.
// Protocol logic lives in components/vallox_protocol and stays free of IDF
// dependencies so it can be unit tested on the host — see firmware/test/host.
//
// There is deliberately no UART task here yet. No board exists, the bus has not
// been captured, and the electrical parameters that decide the pin assignment
// and the driver-enable timing are open measurements — see
// docs/research/measurement-plan.md. Writing a transport against guessed timing
// would only have to be rewritten once the capture exists.

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "vallox_protocol.h"

static const char *TAG = "app";

static vlx_bus_survey_t s_survey;

static void on_frame(const vlx_frame_t *f, void *ctx)
{
    (void)ctx;

    // A six-byte frame has no start delimiter, so a checksum can pass at the
    // wrong alignment. Anything that fails the address sanity check is treated
    // as noise rather than as data.
    if (!vlx_frame_is_plausible(f)) {
        ESP_LOGD(TAG, "implausible frame %02x->%02x reg %02x", f->sender,
                 f->receiver, f->reg);
        return;
    }

    vlx_bus_survey_observe(&s_survey, f);
    ESP_LOGI(TAG, "%02x -> %02x  reg %02x = %02x", f->sender, f->receiver,
             f->reg, f->value);
}

// First thing anyone needs in the field: what is running and why did it restart.
static void log_identity(void)
{
    ESP_LOGI(TAG, "%s firmware %s, reset reason %d, free heap %lu",
             "Vallox RS-485 Controller", FW_VERSION, (int)esp_reset_reason(),
             (unsigned long)esp_get_free_heap_size());
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    log_identity();

    vlx_bus_survey_init(&s_survey);

    static vlx_parser_t parser;
    vlx_parser_init(&parser, on_frame, NULL);

    // Next, once measurements M1-M4 exist: a UART task in
    // UART_MODE_RS485_HALF_DUPLEX feeding vlx_parser_feed_buffer(), calling
    // vlx_parser_reset() on each idle gap, and running passively for long
    // enough that vlx_bus_survey_pick_address() has something to say before
    // anything is transmitted.
}
