#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

struct iNodeMeterData {
  unsigned short rawAvg;
  unsigned int rawSum;
  unsigned short constant : 14;
  unsigned short unit : 2;
  unsigned char lightLevel : 4;
  unsigned char batteryLevel : 4;
  unsigned short weekDayTotal : 12;
  unsigned short weekDay : 4;
} __attribute__((packed));

#define INODE_SET_METHOD(type, name, default) \
  type name##_{default}; \
  void set_##name(type name) { name##_ = name; }

#define INODE_PUBLISH(name, value) \
  do { if (name##_) { name##_->publish_state(value); } } while(0)

// TODO: 1.14.5 misses the `==` which is present on `1.15.x`
inline bool operator == (esphome::esp32_ble_tracker::ESPBTUUID a, esphome::esp32_ble_tracker::ESPBTUUID b) {
  return a.to_string() == b.to_string();
}

class iNodeMeterSensor : public esphome::Component, public esphome::esp32_ble_tracker::ESPBTDeviceListener {
public:
  bool parse_device(const esphome::esp32_ble_tracker::ESPBTDevice &device) override;
  bool parse_meter_device(const esphome::esp32_ble_tracker::ESPBTDevice &device,
    const unsigned char *data, int data_size);
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  INODE_SET_METHOD(uint64_t, address, 0);
  INODE_SET_METHOD(int, constant, 1);

  INODE_SET_METHOD(esphome::sensor::Sensor *, avg_raw, 0);
  INODE_SET_METHOD(esphome::sensor::Sensor *, avg_w, 0);
  INODE_SET_METHOD(esphome::sensor::Sensor *, avg_dm3, 0);

  INODE_SET_METHOD(esphome::sensor::Sensor *, total_raw, 0);
  INODE_SET_METHOD(esphome::sensor::Sensor *, total_kwh, 0);
  INODE_SET_METHOD(esphome::sensor::Sensor *, total_dm3, 0);

  INODE_SET_METHOD(esphome::sensor::Sensor *, battery_level, 0);
  INODE_SET_METHOD(esphome::sensor::Sensor *, battery_level_v, 0);

  INODE_SET_METHOD(esphome::sensor::Sensor *, light_level, 0);
};

#endif
