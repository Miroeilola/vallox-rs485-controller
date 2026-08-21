// SPDX-License-Identifier: MIT
#include "vallox_rs485_controller.h"
#include "esphome/core/log.h"

namespace esphome {
namespace vallox_rs485_controller {

static const char *const TAG = "vallox_rs485_controller";

void ValloxRs485Controller::setup() { device_parser_init(&this->parser_, &ValloxRs485Controller::on_frame, this); }

void ValloxRs485Controller::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) break;
    device_parser_feed(&this->parser_, byte);
  }
}

void ValloxRs485Controller::on_frame(const uint8_t *payload, size_t len, void *ctx) {
  auto *self = static_cast<ValloxRs485Controller *>(ctx);
  (void) self;
  (void) payload;
  ESP_LOGD(TAG, "frame received, %u bytes", static_cast<unsigned>(len));
}

void ValloxRs485Controller::dump_config() {
  ESP_LOGCONFIG(TAG, "Vallox RS-485 Controller:");
  ESP_LOGCONFIG(TAG, "  frames ok: %u, checksum errors: %u",
                this->parser_.stats.frames_ok, this->parser_.stats.checksum_errors);
  this->check_uart_settings(9600);
}

}  // namespace vallox_rs485_controller
}  // namespace esphome
