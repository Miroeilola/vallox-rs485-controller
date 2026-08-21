// SPDX-License-Identifier: MIT
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

extern "C" {
#include "vallox_protocol.h"
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
  static void on_frame(const vlx_frame_t *frame, void *ctx);

  vlx_parser_t parser_{};
  vlx_bus_survey_t survey_{};
  uint32_t last_byte_ms_{0};
};

}  // namespace vallox_rs485_controller
}  // namespace esphome
