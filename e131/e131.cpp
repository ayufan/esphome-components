#include "e131.h"
#include "e131_light_effect.h"
#include "esphome/core/log.h"

#define MAX_UNIVERSES 512
#define TAG "e131"

using namespace esphome;

void E131Component::add_effect(E131LightEffect *light_effect) {
  light_effects_.insert(light_effect);
  should_rebind_ = true;
}

void E131Component::remove_effect(E131LightEffect *light_effect) {
  light_effects_.erase(light_effect);
  should_rebind_ = true;
}

void E131Component::loop() {
  if (should_rebind_) {
    should_rebind_ = false;
    rebind();
  }

  if (!e131_client_) {
    return;
  }

  while (!e131_client_->isEmpty()) {
    e131_packet_t packet;
    e131_client_->pull(&packet); // Pull packet from ring buffer
    auto universe = htons(packet.universe);
    process(universe, packet);
  }
}

void E131Component::rebind() {
  e131_client_.reset();

  int first_universe = MAX_UNIVERSES;
  int last_universe = 0;
  int universe_count = 0;

  for (auto light_effect : light_effects_) {
    first_universe = std::min(first_universe, light_effect->first_universe());
    last_universe = std::max(last_universe, light_effect->last_universe());
    universe_count += light_effect->universe_count();
  }

  if (!universe_count) {
    ESP_LOGI(TAG, "Stopped E1.31.");
    return;
  }

  e131_client_.reset(new ESPAsyncE131(universe_count));

  bool success = e131_client_->begin(
    listen_method_, first_universe, last_universe - first_universe + 1);

  if (success) {
    ESP_LOGI(TAG, "Started E1.31 for universes %d-%d",
      first_universe, last_universe);
  } else {
    ESP_LOGW(TAG, "Failed to start E1.31 for %s universes %d-%d",
      first_universe, last_universe);
  }
}

void E131Component::process(int universe, const e131_packet_t &packet) {
  for (auto light_effect : light_effects_) {
    light_effect->process(universe, packet);
  }
}
