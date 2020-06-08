#pragma once

#include "esphome/core/component.h"

#ifdef ARDUINO_ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include <memory>
#include <set>
#include <map>

class AsyncUDP;
class AsyncUDPPacket;

namespace esphome {
namespace e131 {

class E131AddressableLightEffect;

enum E131ListenMethod { E131_MULTICAST, E131_UNICAST };

struct E131Packet {
  uint16_t count;
  uint8_t values[513];
};

class E131Component : public esphome::Component {
 public:
  E131Component();
  ~E131Component();

  void setup() override;
  void loop() override;

 public:
  void add_effect(E131AddressableLightEffect *light_effect);
  void remove_effect(E131AddressableLightEffect *light_effect);

 public:
  void set_method(E131ListenMethod listen_method) { this->listen_method_ = listen_method; }

 protected:
  void packet_(AsyncUDPPacket &packet);
  bool process_(int universe, const E131Packet &packet);
  void join_(int universe);
  void leave_(int universe);

 protected:
  E131ListenMethod listen_method_{E131_MULTICAST};
  std::unique_ptr<AsyncUDP> udp_;
  std::set<E131AddressableLightEffect *> light_effects_;
  std::map<int, int> universe_consumers_;
  std::map<int, E131Packet> universe_packets_;
#ifdef ARDUINO_ARCH_ESP32
  SemaphoreHandle_t lock_{nullptr};
#endif
};

}  // namespace e131
}  // namespace esphome
