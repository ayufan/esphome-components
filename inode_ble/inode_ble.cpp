#include "inode_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
using namespace esphome;

static const char *TAG = "inode_ble";

bool iNodeMeterSensor::parse_device(
  const esphome::esp32_ble_tracker::ESPBTDevice &device)
{
  if (device.address_uint64() != this->address_)
    return false;

  auto data_size = device.get_manufacturer_data().size();
  if (data_size < 3) {
    return false;
  }

  auto data = (unsigned char*)device.get_manufacturer_data().c_str();
  if (data[0] != 0x90 && data[0] != 0xa0) {
    return false;
  }

  switch(data[1]) {
    case 0x82:
      return parse_meter_device(device, data + 2, data_size - 2);

    default:
      return false;
  }
}

bool iNodeMeterSensor::parse_meter_device(
  const esphome::esp32_ble_tracker::ESPBTDevice &device,
  const unsigned char *data,
  int data_size)
{
  if (data_size < sizeof(iNodeMeterData))
    return false;

  const iNodeMeterData *meter = (const iNodeMeterData *)data;

  auto avg = 60.0f * (unsigned int)meter->rawAvg / constant_;
  auto sum = 1.0f * (unsigned int)meter->rawSum / constant_;
  auto batteryLevel = 10 * ((meter->batteryLevel < 11 ? meter->batteryLevel : 11) - 1);
  auto batteryVoltage = batteryLevel * 1200 / 100 + 1800;
  auto lightLevel = meter->lightLevel * 100 / 15;

  INODE_PUBLISH(this->avg_w, avg * 1000);
  INODE_PUBLISH(this->avg_dm3, avg);
  INODE_PUBLISH(this->avg_raw, meter->rawAvg);
  INODE_PUBLISH(this->total_kwh, sum);
  INODE_PUBLISH(this->total_dm3, sum);
  INODE_PUBLISH(this->total_raw, meter->rawSum);
  INODE_PUBLISH(this->light_level, lightLevel);
  INODE_PUBLISH(this->battery_level, batteryLevel);
  INODE_PUBLISH(this->battery_level_v, batteryVoltage / 1000.0);

  return true;
}

void iNodeMeterSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "iNode Meter");

  LOG_SENSOR("  ", "Avg Raw", this->avg_raw_);
  LOG_SENSOR("  ", "Avg W", this->avg_w_);
  LOG_SENSOR("  ", "Avg dm3", this->avg_dm3_);

  LOG_SENSOR("  ", "Total Raw", this->total_raw_);
  LOG_SENSOR("  ", "Total kW/h", this->total_kwh_);
  LOG_SENSOR("  ", "Total dm3", this->total_dm3_);

  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Battery Level (V)", this->battery_level_v_);

  LOG_SENSOR("  ", "Light Level", this->light_level_);
}

#endif