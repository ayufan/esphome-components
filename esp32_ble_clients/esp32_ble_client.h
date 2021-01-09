#pragma once

#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_gattc_api.h>

#include <map>
#include <queue>
#include <memory>

class ESP32BLELock;

class ESP32BLEClient
{
public:
  uint8_t *readresult_value;
  uint16_t readresult_value_len;
  ESP32BLEClient(uint16_t app_id);
  ~ESP32BLEClient();

public:
  void set_address(uint64_t address);
  void set_address_type(esp_ble_addr_type_t address_type);
  void set_timeout(int timeout_ms);

public:
  enum State {
    Idle,

    Connecting = 10,
    Registered = Connecting,

    Connected = 20,
    Ready = Connected
  };

public:
  bool is_connecting();
  bool is_connected();
  bool is_disconnecting();

public:
  bool connect();
  void disconnect();

public:
  bool request_services(bool force = false);
  uint16_t get_characteristic(const esp_bt_uuid_t &service_uuid, const esp_bt_uuid_t &characteristic_uuid);
  uint16_t get_descriptor(uint16_t characteristic, const esp_bt_uuid_t &uuid);

  enum WriteType {
    Characteristic,
    Descriptor
  };

  bool write(
    WriteType type,
    uint16_t handle,
    void *data, uint16_t data_length,
    bool response);

  bool read(
    uint16_t handle
    );

  bool register_notify(
    uint16_t handle,
    bool enable);

  bool write_notify_desc(
    uint16_t handle,
    bool enable,
    bool notifications);

  struct Notification {
    uint16_t handle{0};
    std::string data;
    bool notification{false};
  };

  std::vector<Notification> wait_for_notifications(int own_timeout_ms);

private:
  bool register_if(ESP32BLELock &lock);
  bool open_if(ESP32BLELock &lock);
  bool mtu_if(ESP32BLELock &lock);
  bool close_if(ESP32BLELock &lock);
  bool unregister_if(ESP32BLELock &lock);

private:
  State state{Idle};
  bool disconnecting{false};

private:
  uint64_t address64{0};
  esp_bd_addr_t address{0,};
  int timeout_ms{5000};
  esp_ble_addr_type_t address_type{BLE_ADDR_TYPE_PUBLIC};
  esphome::optional<uint16_t> app_id;
  esphome::optional<esp_gatt_if_t> gattc_if;
  esphome::optional<uint16_t> conn_id;
  std::vector<esp_ble_gattc_cb_param_t::gattc_search_res_evt_param> services;
  uint16_t mtu{23};

private:
  struct EventResult {
    esp_gattc_cb_event_t type;
    esp_ble_gattc_cb_param_t param;
  };

  typedef std::function<bool(const EventResult&)> EventHandler;

  std::map<esp_gattc_cb_event_t, std::function<void(const EventResult&)> > event_handlers;
  std::queue<std::function<void()> > calls;
  std::vector<Notification> notifications;

private:
  void client_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

  esp_err_t log(
    const char *reason,
    esp_err_t code);

  bool set_state(
    State new_state);

  bool wait_for_event(
    ESP32BLELock &lock,
    esp_gattc_cb_event_t event,
    int own_timeout_ms,
    EventHandler handler);

private:
  SemaphoreHandle_t lock{nullptr};

  friend class ESP32BLE;
};
