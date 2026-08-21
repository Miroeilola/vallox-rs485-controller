// SPDX-License-Identifier: MIT
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

extern "C" {
#include "device_core.h"
}

namespace esphome {
namespace vallox_rs485_controller {

class ValloxRs485Controller : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  static void on_frame(const uint8_t *payload, size_t len, void *ctx);
  device_parser_t parser_{};
};

}  // namespace vallox_rs485_controller
}  // namespace esphome
