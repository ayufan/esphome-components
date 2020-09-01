#include "inode_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
using namespace esphome;

static const char *TAG = "inode_ble";

static const esphome::esp32_ble_tracker::ESPBTUUID emeterDevice =
  esphome::esp32_ble_tracker::ESPBTUUID::from_uint16(0x8290);

static const esphome::esp32_ble_tracker::ESPBTUUID emeterDeviceLR =
  esphome::esp32_ble_tracker::ESPBTUUID::from_uint16(0x82a0);

bool iNodeMeterSensor::parse_device(
  const esphome::esp32_ble_tracker::ESPBTDevice &device)
{
  if (device.address_uint64() != this->address_)
    return false;

  bool parsed = false;

  // iNode Manfacturer Data: UUID: 82:A0. Data: 07.00.2B.24.A0.00.E8.03.B0.00.00 (11)
  for (auto service_data : device.get_manufacturer_datas()) {
    ESP_LOGD(TAG, "iNode Manfacturer Data: UUID: %s. Data: %s",
      service_data.uuid.to_string().c_str(), hexencode(service_data.data).c_str());

    if (service_data.data.empty()) {
      continue;
    }

    if (service_data.uuid == emeterDevice || service_data.uuid == emeterDeviceLR) {
      if (parse_meter_device(device, &service_data.data[0], service_data.data.size())) {
        parsed = true;
      }
    }
  }

  return parsed;
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