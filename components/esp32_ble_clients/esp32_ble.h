#pragma once

#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>

#include <map>

class ESP32BLEClient;

class ESP32BLE
{
public:
  static ESP32BLE &instance();

public:
  ESP32BLE();
  ~ESP32BLE();

public:
  bool ble_setup();
  ESP32BLEClient* acquire();
  bool release(ESP32BLEClient *client);

private:
  void client_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

private:
  static void esp32_ble_client_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

private:
  std::map<uint16_t, ESP32BLEClient*> app_ids;
  std::map<esp_gatt_if_t, uint16_t> gattc_ifs;
  uint16_t next_app_id{1};
  SemaphoreHandle_t lock{nullptr};
  bool initialized{false};

  friend class ESP32BLEClient;
};
