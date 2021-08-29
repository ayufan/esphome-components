#include "memory_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"

namespace esphome {
namespace debug {

static const char *TAG = "memory";

void MemoryComponent::update() {
  uint32_t free_heap = ESP.getFreeHeap();
  ESP_LOGD(TAG, "Free Heap Size: %u bytes", free_heap);
}

float MemoryComponent::get_setup_priority() const {
  return setup_priority::LATE;
}

}  // namespace debug
}  // namespace esphome
