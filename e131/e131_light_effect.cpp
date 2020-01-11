#include "e131.h"
#include "e131_light_effect.h"
#include "esphome/core/log.h"

#define MAX_DATA_SIZE 512
#define TAG "e131_light_effect"

using namespace esphome;

E131LightEffect::E131LightEffect(const std::string &name)
  : AddressableLightEffect(name) {
}

int E131LightEffect::data_per_universe() const {
  return lights_per_universe() * channels_;
}

int E131LightEffect::lights_per_universe() const {
  return MAX_DATA_SIZE / channels_;
}

int E131LightEffect::first_universe() const {
  return first_universe_;
}

int E131LightEffect::last_universe() const {
  return first_universe_ + universe_count() - 1;
}

int E131LightEffect::universe_count() const {
  // Round up to lights_per_universe
  auto lights = lights_per_universe();
  return (get_addressable_()->size() + lights - 1) / lights;
}

void E131LightEffect::start() {
  if (this->e131_) {
    this->e131_->add_effect(this);
  }
}

void E131LightEffect::stop() {
  if (this->e131_) {
    this->e131_->remove_effect(this);
  }
}

void E131LightEffect::apply(esphome::light::AddressableLight &it, const esphome::light::ESPColor &current_color) {
  // ignore, it is run by `::update()`
}

void E131LightEffect::process(int universe, const e131_packet_t &packet) {
  auto it = get_addressable_();

  ESP_LOGV(TAG, "Received E1.31 packet on %s for %d universe, with %d bytes",
    get_name().c_str(),
    universe, packet.property_value_count);

  // check if this is our universe and data are valid
  if (universe < first_universe_ || universe > last_universe())
    return;

  int output_offset = (universe - first_universe_) * lights_per_universe();
  int output_end = std::min(
    output_offset + lights_per_universe(),
    it->size());
  auto input_data = packet.property_values + 1;

  ESP_LOGV(TAG, "Applying data on %d-%d", output_offset, output_end);

  if (channels_ == E131_RGB) {
    for ( ; output_offset < output_end; output_offset++, input_data += 3) {
      auto output = (*it)[output_offset];
      output.set(esphome::light::ESPColor(
        input_data[0], input_data[1], input_data[2], (input_data[0] + input_data[1] + input_data[2]) / 3));
    }
  } else if (channels_ == E131_RGBW) {
    for ( ; output_offset < output_end; output_offset++, input_data += 4) {
      auto output = (*it)[output_offset];
      output.set(esphome::light::ESPColor(
        input_data[0], input_data[1], input_data[2], input_data[3]));
    }
  }
}
