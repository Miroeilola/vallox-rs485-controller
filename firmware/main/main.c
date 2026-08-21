// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Vallox RS-485 Controller — application entry point.
//
// The application layer owns transports (UART, Wi-Fi, MQTT) and lifecycle.
// Protocol logic lives in components/device_core and stays free of IDF
// dependencies so it can be unit tested on the host — see firmware/test/host.

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "device_core.h"

static const char *TAG = "app";

static void on_frame(const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "frame received, %u bytes", (unsigned)len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, payload, len, ESP_LOG_DEBUG);
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

    static device_parser_t parser;
    device_parser_init(&parser, on_frame, NULL);

    // TODO(template): feed parser from the real transport, e.g. a UART read task.
}
