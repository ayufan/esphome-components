#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace zp16 {

class ZP16Component : public PollingComponent, public uart::UARTDevice {
 public:
  ZP16Component() = default;

  /// Manually set the rx-only mode. Defaults to false.
  void set_rx_mode_only(bool rx_mode_only);
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }
  void set_tvoc_scale(sensor::Sensor *tvoc) { tvoc_scale_ = tvoc; }

  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  float get_setup_priority() const override;
  void set_qa_mode(bool qa_mode);

 protected:
  uint8_t zp16_checksum_(const uint8_t *command_data, uint8_t length) const;
  optional<bool> check_byte_() const;
  void parse_data_();

  uint8_t data_[9];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};

  sensor::Sensor *tvoc_{nullptr};
  sensor::Sensor *tvoc_scale_{nullptr};
  bool rx_mode_only_;
};

}  // namespace zp16
}  // namespace esphome