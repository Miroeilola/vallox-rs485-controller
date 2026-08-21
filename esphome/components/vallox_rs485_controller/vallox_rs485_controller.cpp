// SPDX-License-Identifier: MIT
#include "vallox_rs485_controller.h"
#include "esphome/core/log.h"

namespace esphome {
namespace vallox_rs485_controller {

static const char *const TAG = "vallox_rs485_controller";

// Telegrams on this bus are separated by an idle gap as well as by their
// checksum. The real gap has not been measured yet (see
// docs/research/measurement-plan.md, M4), so this is a placeholder that is
// safely longer than one 6-byte frame at 9600 baud (6.25 ms) and shorter than
// any reported poll interval.
static const uint32_t IDLE_GAP_MS = 20;

void ValloxRs485Controller::setup() {
  vlx_parser_init(&this->parser_, &ValloxRs485Controller::on_frame, this);
  vlx_bus_survey_init(&this->survey_);
}

void ValloxRs485Controller::loop() {
  const uint32_t now = millis();

  // An idle gap resolves the byte alignment for free: whatever partial frame is
  // buffered cannot be completed by the next burst.
  if (this->parser_.fill != 0 && (now - this->last_byte_ms_) > IDLE_GAP_MS) {
    vlx_parser_reset(&this->parser_);
  }

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) break;
    this->last_byte_ms_ = now;
    vlx_parser_feed(&this->parser_, byte);
  }
}

void ValloxRs485Controller::on_frame(const vlx_frame_t *frame, void *ctx) {
  auto *self = static_cast<ValloxRs485Controller *>(ctx);

  // Six bytes with no start delimiter means a checksum can pass at the wrong
  // alignment. Address sanity is the cheap second filter.
  if (!vlx_frame_is_plausible(frame)) {
    ESP_LOGV(TAG, "implausible frame %02X->%02X", frame->sender, frame->receiver);
    return;
  }

  vlx_bus_survey_observe(&self->survey_, frame);
  ESP_LOGD(TAG, "%02X -> %02X  reg %02X = %02X", frame->sender, frame->receiver,
           frame->reg, frame->value);
}

void ValloxRs485Controller::dump_config() {
  ESP_LOGCONFIG(TAG, "Vallox RS-485 Controller:");
  ESP_LOGCONFIG(TAG, "  frames ok: %u, checksum rejects: %u, bytes discarded: %u",
                this->parser_.stats.frames_ok, this->parser_.stats.checksum_rejects,
                this->parser_.stats.bytes_discarded);
  const uint8_t other = vlx_bus_survey_other_controller(&this->survey_, 0);
  if (other != 0) {
    ESP_LOGW(TAG, "  another controller is on the bus at 0x%02X - two controllers "
                  "override each other, so this device must stay silent", other);
  } else {
    ESP_LOGCONFIG(TAG, "  panel side of the bus is clear");
  }
  this->check_uart_settings(9600);
}

}  // namespace vallox_rs485_controller
}  // namespace esphome
