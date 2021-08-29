#include "esp32_ble_log.h"
#include "esp32_ble_lock.h"
#include "esp32_ble_client.h"
#include "esp32_ble.h"

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <esp_coexist.h>

static const char *TAG = "esp32_ble";
static const int MAX_CLIENTS = 1;

using namespace esphome;

ESP32BLE& ESP32BLE::instance() {
  static ESP32BLE clients;
  return clients;
}

ESP32BLE::ESP32BLE()
{
  lock = xSemaphoreCreateMutex();
  initialized = ble_setup();

  if (auto err = esp_ble_gattc_register_callback(esp32_ble_client_event_handler)) {
    ESP_LOGE(TAG, "esp_ble_gattc_register_callback: %x", err);
  }
}

ESP32BLE::~ESP32BLE()
{
  esp_ble_gattc_register_callback(nullptr);

  if (initialized) {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
  }

  vSemaphoreDelete(lock);
}

bool ESP32BLE::ble_setup()
{
  if (btStarted()) {
    return true;
  }

#if 0
  if (!btStart2()) {
    ESP_LOGD(TAG, "btStart failed.");
    return false;
  }
#else
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  cfg.mode = ESP_BT_MODE_BLE;

  if (auto err = esp_bt_controller_init(&cfg)) {
    ESP_LOGD(TAG, "esp_bt_controller_init: %d", err);
    ESP_LOGD(TAG, "esp_bt_controller_get_status: %d", esp_bt_controller_get_status());
    return false;
  }

  if (auto err = esp_bt_controller_enable((esp_bt_mode_t)cfg.mode)) {
    ESP_LOGD(TAG, "esp_bt_controller_enable: %d", err);
    ESP_LOGD(TAG, "esp_bt_controller_get_status: %d", esp_bt_controller_get_status());
    return false;
  }
#endif

  if (auto err = esp_bluedroid_init()) {
    ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", err);
    return false;
  }

  if (auto err = esp_bluedroid_enable()) {
    ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", err);
    return false;
  }

  // Empty name
  esp_ble_gap_set_device_name("");

  esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
  if (auto err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t))) {
    ESP_LOGE(TAG, "esp_ble_gap_set_security_param failed: %d", err);
    return false;
  }

  if (auto err = esp_ble_gatt_set_local_mtu(500)) {
    ESP_LOGE(TAG, "esp_ble_gatt_set_local_mtu: %x", err);
    return false;
  }

  // BLE takes some time to be fully set up, 200ms should be more than enough
  delay(200);  // NOLINT

  ESP_LOGD(TAG, "BT init done.");

  return true;
}

ESP32BLEClient* ESP32BLE::acquire()
{
  ESP32BLELock lock(this->lock);

  if (!initialized) {
    return nullptr;
  }

  // wait till we really free gattc_ifs
  if (gattc_ifs.size() >= MAX_CLIENTS) {
    return nullptr;
  }

  // find next free app_id
  while (app_ids[next_app_id])
    next_app_id++;

  auto client = new ESP32BLEClient(next_app_id);
  app_ids[next_app_id] = client;
  return client;
}

bool ESP32BLE::release(ESP32BLEClient *client)
{
  ESP32BLELock lock(this->lock);

  for (auto iter = app_ids.begin(); iter != app_ids.end(); ++iter) {
    if (iter->second == client) {
      app_ids.erase(iter);
      return true;
    }
  }

  return false;
}

void ESP32BLE::esp32_ble_client_event_handler(
  esp_gattc_cb_event_t event,
  esp_gatt_if_t gattc_if,
  esp_ble_gattc_cb_param_t* param)
{
  instance().client_event_handler(event, gattc_if, param);
}

void ESP32BLE::client_event_handler(
  esp_gattc_cb_event_t event,
  esp_gatt_if_t gattc_if,
  esp_ble_gattc_cb_param_t* param)
{
  ESP32BLEClient *client = nullptr;

  {
    ESP32BLELock lock(this->lock);

    // Register gattc_if
    if (event == ESP_GATTC_REG_EVT) {
      gattc_ifs[gattc_if] = param->reg.app_id;
    }

    client = app_ids[gattc_ifs[gattc_if]];

    // Unregister gattc_if
    if (event == ESP_GATTC_UNREG_EVT) {
      gattc_ifs.erase(gattc_if);
    }
  }

  if (!client) {
    // We log with `printf` as the `ESP_LOG*` is not thread-safe
    BLE_LOGD(TAG, "EventHandler: event=%x, gattc_if=%x => client not found\n",
      event, gattc_if);

    // Cleanup and disconnect client if for whatever reason it got connected
    switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status == ESP_OK) {
        esp_ble_gattc_close(gattc_if, param->open.conn_id);
      } else {
        esp_ble_gattc_app_unregister(gattc_if);
      }
      break;

    case ESP_GATTC_CLOSE_EVT:
      esp_ble_gattc_app_unregister(gattc_if);
      break;

    default:
      break;
    }
    return;
  }

  client->client_event_handler(event, gattc_if, param);
}
