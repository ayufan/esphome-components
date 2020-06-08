#include "e131.h"
#include "e131_addressable_light_effect.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncUDP.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncUDP.h>
#endif

namespace esphome {
namespace e131 {

static const char *TAG = "e131";
static const int PORT = 5568;

E131Component::E131Component() {
#ifdef ARDUINO_ARCH_ESP32
  lock_ = xSemaphoreCreateMutex();
#endif
}

E131Component::~E131Component() {
#ifdef ARDUINO_ARCH_ESP32
  vSemaphoreDelete(lock_);
#endif
}

void E131Component::setup() {
  udp_.reset(new AsyncUDP());
  udp_->onPacket([this](AsyncUDPPacket &packet) { this->packet_(packet); });

  if (!udp_->listen(PORT)) {
    ESP_LOGE(TAG, "Cannot bind E1.31 to %d.", PORT);
    mark_failed();
  }
}

void E131Component::loop() {
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreTake(lock_, portMAX_DELAY);
#endif
  std::map<int, E131Packet> packets;
  packets.swap(universe_packets_);
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreGive(lock_);
#endif

  for (auto packet : packets) {
    auto handled = process_(packet.first, packet.second);

    if (!handled) {
      ESP_LOGV(TAG, "Ignored packet for %d universe of size %d.", packet.first, packet.second.count);
    }
  }
}

void E131Component::add_effect(E131AddressableLightEffect *light_effect) {
  if (light_effects_.count(light_effect)) {
    return;
  }

  ESP_LOGD(TAG, "Registering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.insert(light_effect);

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    join_(universe);
  }
}

void E131Component::remove_effect(E131AddressableLightEffect *light_effect) {
  if (!light_effects_.count(light_effect)) {
    return;
  }

  ESP_LOGD(TAG, "Unregistering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.erase(light_effect);

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    leave_(universe);
  }
}

bool E131Component::process_(int universe, const E131Packet &packet) {
  bool handled = false;

  ESP_LOGV(TAG, "Received E1.31 packet for %d universe, with %d bytes", universe, packet.count);

  for (auto light_effect : light_effects_) {
    handled = light_effect->process_(universe, packet) || handled;
  }

  return handled;
}

}  // namespace e131
}  // namespace esphome
