#include "e131.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncUDP.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncUDP.h>
#endif

#include <lwip/ip_addr.h>
#include <lwip/igmp.h>

#if LWIP_VERSION_MAJOR == 1
typedef struct ip_addr ip4_addr_t;
#endif

namespace esphome {
namespace e131 {

static const char *TAG = "e131";

static const uint8_t ACN_ID[12] = {0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00};
static const uint32_t VECTOR_ROOT = 4;
static const uint32_t VECTOR_FRAME = 2;
static const uint8_t VECTOR_DMP = 2;

// E1.31 Packet Structure
union E131RawPacket {
  struct {
    // Root Layer
    uint16_t preamble_size;
    uint16_t postamble_size;
    uint8_t acn_id[12];
    uint16_t root_flength;
    uint32_t root_vector;
    uint8_t cid[16];

    // Frame Layer
    uint16_t frame_flength;
    uint32_t frame_vector;
    uint8_t source_name[64];
    uint8_t priority;
    uint16_t reserved;
    uint8_t sequence_number;
    uint8_t options;
    uint16_t universe;

    // DMP Layer
    uint16_t dmp_flength;
    uint8_t dmp_vector;
    uint8_t type;
    uint16_t first_address;
    uint16_t address_increment;
    uint16_t property_value_count;
    uint8_t property_values[513];
  } __attribute__((packed));

  uint8_t raw[638];
};

void E131Component::join_(int universe) {
  // store only latest received packet for the given universe
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreTake(lock_, portMAX_DELAY);
#endif

  auto consumers = ++universe_consumers_[universe];

#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreGive(lock_);
#endif

  if (consumers > 1) {
    return;  // we already joined before
  }

  if (listen_method_ == E131_MULTICAST) {
    ip4_addr_t multicast_addr = {
        static_cast<uint32_t>(IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff)))};

    auto err = igmp_joingroup(IP4_ADDR_ANY4, &multicast_addr);

    if (err) {
      ESP_LOGW(TAG, "IGMP join for %d universe of E1.31 failed. Multicast might not work.", universe);
    }
  }

  ESP_LOGD(TAG, "Joined %d universe for E1.31.", universe);
}

void E131Component::leave_(int universe) {
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreTake(lock_, portMAX_DELAY);
#endif

  auto consumers = --universe_consumers_[universe];

#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreGive(lock_);
#endif

  if (consumers > 0) {
    return;  // we have other consumers of the given universe
  }

  if (listen_method_ == E131_MULTICAST) {
    ip4_addr_t multicast_addr = {
        static_cast<uint32_t>(IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff)))};

    igmp_leavegroup(IP4_ADDR_ANY4, &multicast_addr);
  }

  ESP_LOGD(TAG, "Left %d universe for E1.31.", universe);
}

void E131Component::packet_(AsyncUDPPacket &packet) {
  auto sbuff = reinterpret_cast<E131RawPacket *>(packet.data());
  if (memcmp(sbuff->acn_id, ACN_ID, sizeof(sbuff->acn_id)) != 0)
    return;
  if (htonl(sbuff->root_vector) != VECTOR_ROOT)
    return;
  if (htonl(sbuff->frame_vector) != VECTOR_FRAME)
    return;
  if (sbuff->dmp_vector != VECTOR_DMP)
    return;
  if (sbuff->property_values[0] != 0)
    return;

  auto universe = htons(sbuff->universe);

  // store only latest received packet for the given universe
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreTake(lock_, portMAX_DELAY);
#endif

  if (universe_consumers_[universe] > 0) {
    // store only latest received packet for the given universe
    E131Packet &e131_packet = universe_packets_[universe];
    e131_packet.count = sbuff->property_value_count;
    memcpy(e131_packet.values, sbuff->property_values, sizeof(e131_packet.values));
  }

#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreGive(lock_);
#endif
}

}  // namespace e131
}  // namespace esphome
