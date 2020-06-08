#include "adalight_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adalight {

static const char *TAG = "adalight_light_effect";

static const uint32_t ADALIGHT_ACK_INTERVAL = 1000;
static const uint32_t ADALIGHT_RECEIVE_TIMEOUT = 1000;

class MyUART : public uart::UARTComponent {
public:
  size_t set_rx_buffer_size(size_t size) {
    if (this->hw_serial_) {
      return this->hw_serial_->setRxBufferSize(size);
    } else {
      return 0;
    }
  }
};

AdalightLightEffect::AdalightLightEffect(const std::string &name) : AddressableLightEffect(name) {}

void AdalightLightEffect::start() {
  AddressableLightEffect::start();

  last_ack_ = 0;
  last_byte_ = 0;
  last_reset_ = 0;

  if (this->parent_) {
    static_cast<MyUART *>(this->parent_)->set_rx_buffer_size(1024);
  }
}

void AdalightLightEffect::stop() {
  frame_.resize(0);

  AddressableLightEffect::stop();
}

int AdalightLightEffect::get_frame_size_(int led_count) const {
  // 3 bytes: Ada
  // 2 bytes: LED count
  // 1 byte: checksum
  // 3 bytes per LED
  return 3 + 2 + 1 + led_count * 3;
}

void AdalightLightEffect::reset_frame_(light::AddressableLight &it) {
  int buffer_capacity = get_frame_size_(it.size());

  frame_.clear();
  frame_.reserve(buffer_capacity);
}

void AdalightLightEffect::blank_all_leds(light::AddressableLight &it) {
  for (int led = it.size(); led-- > 0; ) {
    it[led].set(light::ESPColor::BLACK);
  }
}

void AdalightLightEffect::apply(light::AddressableLight &it, const light::ESPColor &current_color) {
  const uint32_t now = millis();

  if (now - this->last_ack_ >= ADALIGHT_ACK_INTERVAL) {
    ESP_LOGV(TAG, "Sending ACK");
    this->write_str("Ada\n");
    this->last_ack_ = now;
  }

  if (!this->last_reset_) {
    ESP_LOGW(TAG, "Frame: Reset.");
    reset_frame_(it);
    blank_all_leds(it);
    this->last_reset_ = now;
  }

  if (this->frame_.size() && now - this->last_byte_ >= ADALIGHT_RECEIVE_TIMEOUT) {
    ESP_LOGW(TAG, "Frame: Receive timeout (size=%d).", this->frame_.size());
    reset_frame_(it);
    blank_all_leds(it);
  }

  if (this->available() > 0) {
    ESP_LOGV(TAG, "Frame: Available (size=%d).", this->available());
  }

  while (this->available() != 0) {
    uint8_t data;
    if (!this->read_byte(&data))
      break;
    this->frame_.push_back(data);
    this->last_byte_ = now;

    switch (this->parse_frame_(it)) {
    case Invalid:
      ESP_LOGD(TAG, "Frame: Invalid (size=%d, first=%d).",
        this->frame_.size(), this->frame_[0]);
      reset_frame_(it);
      break;

    case Partial:
      break;

    case Consumed:
      ESP_LOGV(TAG, "Frame: Consumed (size=%d).", this->frame_.size());
      reset_frame_(it);
      break;
    }
  }
}

AdalightLightEffect::Frame AdalightLightEffect::parse_frame_(light::AddressableLight &it) {
  if (frame_.empty())
    return Invalid;

  // Check header: `Ada`
  if (frame_[0] != 'A')
    return Invalid;
  if (frame_.size() > 1 && frame_[1] != 'd')
    return Invalid;
  if (frame_.size() > 2 && frame_[2] != 'a')
    return Invalid;
  
  // 3 bytes: Count Hi, Count Lo, Checksum
  if (frame_.size() < 6)
    return Partial;

  // Check checksum
  uint16_t checksum = frame_[3] ^ frame_[4] ^ 0x55;
  if (checksum != frame_[5])
    return Invalid;

  // Check if we received the full frame
  uint16_t led_count = (frame_[3] << 8) + frame_[4] + 1;
  auto buffer_size = get_frame_size_(led_count);
  if (frame_.size() < buffer_size)
    return Partial;

  // Apply lights
  auto accepted_led_count = std::min<int>(led_count, it.size());
  uint8_t *led_data = &frame_[6];

  for (int led = 0; led < accepted_led_count; led++, led_data += 3) {
    auto white = std::min(std::min(led_data[0], led_data[1]), led_data[2]);

    it[led].set(light::ESPColor(
      led_data[0], led_data[1], led_data[2], white));
  }

  return Consumed;
}

}  // namespace adalight
}  // namespace esphome
