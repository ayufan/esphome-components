#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light_effect.h"

#include "ESPAsyncE131.h"

namespace esphome {
namespace e131 {

class E131Component;

enum E131LightChannels {
  E131_MONO = 1,
  E131_RGB = 3,
  E131_RGBW = 4
};

class E131AddressableLightEffect : public esphome::light::AddressableLightEffect {
public:
  E131AddressableLightEffect(const std::string &name);

public:
  void start() override;
  void stop() override;
  void apply(esphome::light::AddressableLight &it, const esphome::light::ESPColor &current_color) override;

public:
  int get_data_per_universe() const;
  int get_lights_per_universe() const;
  int get_first_universe() const;
  int get_last_universe() const;
  int get_universe_count() const;

public:
  void set_first_universe(int universe) {
    this->first_universe_ = universe;
  }

  void set_channels(E131LightChannels channels) {
    this->channels_ = channels;
  }

  void set_addressable_light(esphome::light::AddressableLight *addressable_light) {
    this->addressable_light_ = addressable_light;
  }

  void set_e131(E131Component *e131) {
    this->e131_ = e131;
  }

private:
  bool process(int universe, const e131_packet_t &packet);

private:
  int first_universe_{0};
  int last_universe_{0};
  E131LightChannels channels_{E131_RGB};
  esphome::light::AddressableLight *addressable_light_{nullptr};
  E131Component *e131_{nullptr};

  friend class E131Component;
};

}
}
