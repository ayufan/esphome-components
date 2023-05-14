#include "zp16.h"
#include "esphome/core/log.h"

// Based on: https://www.winsentech.com/npublic/opdfjs/web/viewer.html?u=1621050968327720960&fileName=ZP16%20Air-Quality%20Detection%20Module%20manual%201.1

namespace esphome {
namespace zp16 {

static const char *const TAG = "zp16";

static const uint8_t ZP16_DATA_REQUEST_LENGTH = 9;
static const uint8_t ZP16_BYTE0_MSG_HEAD = 0xFF;
static const uint8_t ZP16_BYTE1_TYPE_TVOC = 0x34;
static const uint8_t ZP16_BYTE1_TYPE_COMMAND = 0x86;
static const uint8_t ZP16_BYTE2_SWITCH_COMMAND = 0x78;
static const uint8_t ZP16_BYTE2_QA_COMMAND = 0x86;
static const uint8_t ZP16_BYTE3_SWITCH_COMMAND_ACTIVE_UPLOAD = 0x40;
static const uint8_t ZP16_BYTE3_SWITCH_COMMAND_QA = 0x41;
static const uint8_t ZP16_BYTE2_UNIT_MG_M3 = 0x11;

void ZP16Component::setup() {
  if (this->rx_mode_only_) {
    // In RX-only mode we do not setup the sensor, it is assumed to be setup
    // already
    return;
  }

  set_qa_mode(this->get_update_interval() > 0);
}

void ZP16Component::set_qa_mode(bool qa_mode) {
  if (this->rx_mode_only_) {
    // In RX-only mode we do not setup the sensor, it is assumed to be setup
    // already
    return;
  }

  uint8_t command_data[ZP16_DATA_REQUEST_LENGTH] = {0};
  command_data[0] = ZP16_BYTE0_MSG_HEAD;
  command_data[2] = ZP16_BYTE2_SWITCH_COMMAND;
  command_data[3] = qa_mode ? ZP16_BYTE3_SWITCH_COMMAND_QA : ZP16_BYTE3_SWITCH_COMMAND_ACTIVE_UPLOAD;
  command_data[8] = this->zp16_checksum_(command_data, sizeof(command_data));
  this->write_array(command_data, sizeof(command_data));
}

void ZP16Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ZP16:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "TVOC Sensor", this->tvoc_);
  LOG_SENSOR("  ", "TVOC Scale Sensor", this->tvoc_scale_);
  ESP_LOGCONFIG(TAG, "  RX-only mode: %s", ONOFF(this->rx_mode_only_));
  this->check_uart_settings(9600);
}

void ZP16Component::update() {
  if (this->rx_mode_only_) {
    return;
  }

  if (this->get_update_interval() > 0) {
    uint8_t command_data[ZP16_DATA_REQUEST_LENGTH] = {0};
    command_data[0] = ZP16_BYTE0_MSG_HEAD;
    command_data[2] = ZP16_BYTE2_QA_COMMAND;
    command_data[8] = this->zp16_checksum_(command_data, sizeof(command_data));
    this->write_array(command_data, sizeof(command_data));
  }
}

void ZP16Component::loop() {
  const uint32_t now = millis();
  if ((now - this->last_transmission_ >= 500) && this->data_index_) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGV(TAG, "Last transmission too long ago. Reset RX index.");
    this->data_index_ = 0;
  }

  if (this->available() == 0) {
    return;
  }

  this->last_transmission_ = now;
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      // finished
      this->parse_data_();
      this->data_index_ = 0;
    } else if (!*check) {
      // wrong data
      ESP_LOGV(TAG, "Byte %i of received data frame is invalid.", this->data_index_);
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}

float ZP16Component::get_setup_priority() const { return setup_priority::DATA; }

void ZP16Component::set_rx_mode_only(bool rx_mode_only) { this->rx_mode_only_ = rx_mode_only; }

uint8_t ZP16Component::zp16_checksum_(const uint8_t *command_data, uint8_t length) const {
  uint8_t sum = 0;
  for (uint8_t i = 1; i < length-1; i++) {
    sum += command_data[i];
  }
  return (~sum)+1;
}

optional<bool> ZP16Component::check_byte_() const {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  switch (index) {
    case 0:
      return byte == ZP16_BYTE0_MSG_HEAD;

    case 1:
      return byte == ZP16_BYTE1_TYPE_TVOC || ZP16_BYTE1_TYPE_COMMAND;

    case 8:
      {
        uint8_t checksum = zp16_checksum_(this->data_, this->data_index_ + 1);
        if (checksum != byte) {
          ESP_LOGW(TAG, "ZP16 Checksum doesn't match: 0x%02X!=0x%02X", byte, checksum);
          return false;
        }
        return {};
      }

    default:
      return true;
  }

  return {};
}

void ZP16Component::parse_data_() {
  this->status_clear_warning();

  if (this->data_[1] == ZP16_BYTE1_TYPE_TVOC && this->data_[2] == ZP16_BYTE2_UNIT_MG_M3) {
    unsigned short full_scale = (this->data_[6] << 8) + this->data_[7];
    unsigned short gas_concentration = (this->data_[4] << 8) + this->data_[5];
    unsigned char decimal_point = this->data_[3];

    if (this->tvoc_ != nullptr) {
      this->tvoc_->publish_state((float)gas_concentration / decimal_point);
    }
    if (this->tvoc_scale_ != nullptr) {
      this->tvoc_scale_->publish_state((float)full_scale / decimal_point);
    }
  } else if (this->data_[1] == ZP16_BYTE1_TYPE_COMMAND) {
    unsigned short full_scale = (this->data_[6] << 8) + this->data_[7];
    unsigned short gas_concentration = (this->data_[2] << 8) + this->data_[3];
    unsigned char decimal_point = this->data_[4];

    if (this->tvoc_ != nullptr) {
      this->tvoc_->publish_state((float)gas_concentration / decimal_point);
    }
    if (this->tvoc_scale_ != nullptr) {
      this->tvoc_scale_->publish_state((float)full_scale / decimal_point);
    }
  }
}

}  // namespace zp16
}  // namespace esphome