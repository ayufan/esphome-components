#include "esphome/core/helpers.h"

#include "esp32_ble.h"
#include "esp32_ble_log.h"
#include "esp32_ble_lock.h"
#include "esp32_ble_client.h"

static const char *TAG = "esp32_ble_client";

using namespace esphome;

static bool compare_uuid(const esp_bt_uuid_t &uuid1, const esp_bt_uuid_t &uuid2)
{
  return uuid1.len == uuid2.len &&
    memcmp(uuid1.uuid.uuid128, uuid2.uuid.uuid128, uuid1.len) == 0;
}

static void print_uuid(const char *name, const esp_bt_uuid_t &uuid, uint16_t handle)
{
  BLE_LOGI(TAG, "UUID: %s: %s => handle: %04x",
    name,
    hexencode(uuid.uuid.uuid128, uuid.len).c_str(),
    handle);
}

#define GATT_LOG(call) \
  log(#call, call)

ESP32BLEClient::ESP32BLEClient(uint16_t app_id)
{
  this->lock = xSemaphoreCreateMutex();
  this->app_id = app_id;

  event_handlers[ESP_GATTC_SEARCH_RES_EVT] = [this](const EventResult &result) {
    services.push_back(result.param.search_res);
  };

  event_handlers[ESP_GATTC_CLOSE_EVT] = [this](const EventResult &result) {
    conn_id.reset();
    state = Registered;
  };

  event_handlers[ESP_GATTC_NOTIFY_EVT] = [this](const EventResult &result) {
    Notification notification;
    notification.handle = result.param.notify.handle;
    notification.data.assign((const char*)result.param.notify.value, result.param.notify.value_len);
    notification.notification = result.param.notify.is_notify;
    notifications.push_back(notification);
  };
}

ESP32BLEClient::~ESP32BLEClient()
{
  disconnect();
  ESP32BLE::instance().release(this);
  vSemaphoreDelete(lock);
}

void ESP32BLEClient::set_address(uint64_t address)
{
  this->address64 = address;
  this->address[0] = uint8_t(address >> 40);
  this->address[1] = uint8_t(address >> 32);
  this->address[2] = uint8_t(address >> 24);
  this->address[3] = uint8_t(address >> 16);
  this->address[4] = uint8_t(address >> 8);
  this->address[5] = uint8_t(address >> 0);
}

void ESP32BLEClient::set_address_type(esp_ble_addr_type_t address_type)
{
  this->address_type = address_type;
}

esp_err_t ESP32BLEClient::log(const char *reason, esp_err_t code)
{
  if (code != ESP_OK) {
    BLE_LOGE(TAG, "FAILURE[%10llx]: %s => code=%x\n", address64, reason, code);
  } else {
    BLE_LOGI(TAG, "SUCCESS[%10llx]: %s\n", address64, reason);
  }
  BLE_LOGD(TAG, "FREE_HEAP: %d\n", ESP.getFreeHeap());

  return code;
}

bool ESP32BLEClient::is_connecting()
{
  return Connecting <= state && state < Connected && !disconnecting;
}

bool ESP32BLEClient::is_connected()
{
  return Connected <= state && !disconnecting;
}

bool ESP32BLEClient::is_disconnecting()
{
  return disconnecting;
}

bool ESP32BLEClient::connect()
{
  ESP32BLELock lock(this->lock);

  register_if(lock);
  open_if(lock);
  mtu_if(lock);
  return is_connected();
}

void ESP32BLEClient::disconnect()
{
  ESP32BLELock lock(this->lock);

  close_if(lock);
  unregister_if(lock);
}

bool ESP32BLEClient::wait_for_event(
  ESP32BLELock &lock,
  esp_gattc_cb_event_t event,
  int own_timeout_ms,
  EventHandler handler)
{
  int end = millis() + own_timeout_ms;
  volatile bool done = false;
  volatile bool result = false;

  auto previous_event = event_handlers[event];
  event_handlers[event] = [this,&done,&result,event,handler](const EventResult &event_result) {
    result = handler(event_result);
    done = true;
  };

  lock.give();

  while(!done && millis() < end) {
    delay(10);
  }

  BLE_LOGD(TAG, "EVENT:%d, !!!! done here? %d => result: %d\n", event, done, result);

  lock.take();
  event_handlers[event] = previous_event;
  return result;
}

std::vector<ESP32BLEClient::Notification> ESP32BLEClient::wait_for_notifications(
  int own_timeout_ms)
{
  ESP32BLELock lock(this->lock);

  int end = millis() + own_timeout_ms;

  while(notifications.empty() && millis() < end) {
    lock.give();
    delay(10);
    lock.take();
  }

  auto current = notifications;
  notifications.clear();
  return current;
}

bool ESP32BLEClient::register_if(ESP32BLELock &lock)
{
  if (state != Idle) {
    return false;
  }

  auto ret = GATT_LOG(esp_ble_gattc_app_register(*app_id));
  if (ret) {
    return false;
  }

  return wait_for_event(lock, ESP_GATTC_REG_EVT, timeout_ms, [this](const EventResult &result) {
    if (GATT_LOG(result.param.reg.status) == ESP_GATT_OK) {
      set_state(Registered);
      return true;
    } else {
      return false;
    }
  });
}

bool ESP32BLEClient::open_if(ESP32BLELock &lock)
{
  if (state != Registered) {
    return false;
  }

  auto ret = GATT_LOG(esp_ble_gattc_open(
    *gattc_if, address, address_type, 1));
  if (ret) {
    return false;
  }

  services.clear();
  notifications.clear();

  return wait_for_event(lock, ESP_GATTC_OPEN_EVT, timeout_ms, [this](const EventResult &result) {
    if (GATT_LOG(result.param.open.status) == ESP_GATT_OK) {
      this->conn_id = result.param.open.conn_id;
      this->mtu = result.param.open.mtu;
      set_state(Ready);
      return true;
    } else {
      return false;
    }
  });
}

bool ESP32BLEClient::mtu_if(ESP32BLELock &lock)
{
  if (state != Connected) {
    return false;
  }

  auto ret = GATT_LOG(esp_ble_gattc_send_mtu_req(
      *gattc_if, *conn_id));
  if (ret) {
    return false;
  }

  return wait_for_event(lock, ESP_GATTC_CFG_MTU_EVT, timeout_ms, [this](const EventResult &result) {
    if (GATT_LOG(result.param.cfg_mtu.status) == ESP_OK) {
      mtu = result.param.cfg_mtu.mtu;
      return true;
    }

    return false;
  });
}

bool ESP32BLEClient::close_if(ESP32BLELock &lock)
{
  if (state != Ready) {
    return false;
  }

  notifications.clear();

  auto ret = GATT_LOG(esp_ble_gattc_close(*gattc_if, *conn_id));
  if (ret) {
    return false;
  }

  return wait_for_event(lock, ESP_GATTC_CLOSE_EVT, timeout_ms, [this](const EventResult &result) {
    if (GATT_LOG(result.param.close.status) == ESP_GATT_OK) {
      conn_id.reset();
      services.clear();
      set_state(Registered);
      return true;
    } else {
      return false;
    }
  });
}

bool ESP32BLEClient::unregister_if(ESP32BLELock &lock)
{
  if (state != Registered) {
    return false;
  }

  auto ret = GATT_LOG(esp_ble_gattc_app_unregister(*gattc_if));
  if (ret) {
    return false;
  }

  return wait_for_event(lock, ESP_GATTC_UNREG_EVT, timeout_ms, [this](const EventResult &result) {
    gattc_if.reset();
    set_state(Idle);
    return true;
  });
}

bool ESP32BLEClient::request_services(bool force)
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return false;
  }

  if (!services.empty() && !force) {
    return true;
  }

  services.clear();

  auto ret = GATT_LOG(esp_ble_gattc_search_service(
    *gattc_if, *conn_id, nullptr));
  if (ret != ESP_OK) {
    return false;
  }

  return wait_for_event(lock, ESP_GATTC_SEARCH_CMPL_EVT, timeout_ms, [this](const EventResult &result) {
    return GATT_LOG(result.param.search_cmpl.status) == ESP_GATT_OK;
  });
}

uint16_t ESP32BLEClient::get_characteristic(const esp_bt_uuid_t &service_uuid, const esp_bt_uuid_t &characteristic_uuid)
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return 0;
  }

  // Look for service first
  esp_ble_gattc_cb_param_t::gattc_search_res_evt_param *service = nullptr;

  for (auto it = services.begin(); it != services.end(); ++it) {
    if (compare_uuid(service_uuid, it->srvc_id.uuid)) {
      service = &*it;
      break;
    }
  }

  if (!service) {
    return 0;
  }
  print_uuid("SERVICE", service->srvc_id.uuid, service->start_handle);
  print_uuid("SERVICE", service->srvc_id.uuid, service->end_handle);

  esp_gattc_char_elem_t result;

  // Look for characterstic
  uint16_t count = 1;
  auto status = GATT_LOG(::esp_ble_gattc_get_char_by_uuid(
    *gattc_if, *conn_id,
    service->start_handle, service->end_handle,
    characteristic_uuid,
    &result, &count));
  if (status != ESP_GATT_OK) {
    return 0;
  }
  if (count == 0) {
    return 0;
  }

  print_uuid("CHAR", result.uuid, result.char_handle);
  if (compare_uuid(characteristic_uuid, result.uuid)) {
    return result.char_handle;
  }

  return 0;
}

uint16_t ESP32BLEClient::get_descriptor(
  uint16_t characteristic, const esp_bt_uuid_t &uuid)
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return 0;
  }

  esp_gattc_descr_elem_t result;

  for(uint16_t offset = 0; ; offset++) {
    uint16_t count = 1;
    auto status = GATT_LOG(::esp_ble_gattc_get_all_descr(
      *gattc_if, *conn_id, characteristic,
      &result, &count, offset));
    if (status != ESP_GATT_OK) {
      break;
    }
    if (count == 0) {
      break;
    }

    if (compare_uuid(uuid, result.uuid)) {
      return result.handle;
    }
  }

  return 0;
}

bool ESP32BLEClient::write(
  WriteType type, uint16_t handle,
  void *data, uint16_t data_length, bool response)
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return false;
  }

  auto write_func = esp_ble_gattc_write_char;
  auto write_event = ESP_GATTC_WRITE_CHAR_EVT;

  if (type == Descriptor) {
    write_func = esp_ble_gattc_write_char_descr;
    write_event = ESP_GATTC_WRITE_DESCR_EVT;
  }

  auto ret = GATT_LOG(write_func(
    *gattc_if, *conn_id, handle, data_length, (uint8_t*)data,
    response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP,
    ESP_GATT_AUTH_REQ_NONE
  ));
  if (ret != ESP_OK) {
    return false;
  }

  return wait_for_event(lock, write_event, timeout_ms, [this](const EventResult &result) {
    return GATT_LOG(result.param.write.status) == ESP_GATT_OK;
  });
}

bool ESP32BLEClient::read(
  uint16_t handle
  )
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return false;
  }

  auto read_func = esp_ble_gattc_read_char;
  auto read_event = ESP_GATTC_READ_CHAR_EVT;

  auto ret = GATT_LOG(read_func(
    *gattc_if, *conn_id, handle, 
    ESP_GATT_AUTH_REQ_NONE
  ));
  if (ret != ESP_OK) {
    return false;
  }
 
  return wait_for_event(lock, read_event, timeout_ms, [this](const EventResult &result) {
    this->readresult_value = result.param.read.value;
    this->readresult_value_len = result.param.read.value_len;
    return GATT_LOG(result.param.write.status) == ESP_GATT_OK;
  });
}

bool ESP32BLEClient::register_notify(
  uint16_t handle, bool enable)
{
  ESP32BLELock lock(this->lock);

  if (state != Ready) {
    return false;
  }

  auto notify_func = esp_ble_gattc_register_for_notify;
  auto notify_event = ESP_GATTC_REG_FOR_NOTIFY_EVT;

  if (!enable) {
    notify_func = esp_ble_gattc_unregister_for_notify;
    notify_event = ESP_GATTC_UNREG_FOR_NOTIFY_EVT;
  }

  auto ret = GATT_LOG(notify_func(
    *gattc_if, address, handle));
  if (ret != ESP_OK) {
    return false;
  }

  return wait_for_event(lock, notify_event, timeout_ms, [this](const EventResult &result) {
    return GATT_LOG(result.param.reg_for_notify.status) == ESP_GATT_OK;
  });
}

bool ESP32BLEClient::write_notify_desc(
  uint16_t handle, bool enable, bool notifications)
{
  std::unique_ptr<esp_bt_uuid_t> uuid(new esp_bt_uuid_t);
  uuid->len = ESP_UUID_LEN_16;
  uuid->uuid.uuid16 = 0x2902;
  auto desc_handle = get_descriptor(handle, *uuid.get());
  if (!desc_handle) {
    return false;
  }

  uint8_t desc_val[] = {
    uint8_t(enable ? (notifications ? 0x01 : 0x02) : 0x0),
    0x0
  };

  return write(Descriptor, desc_handle, desc_val, sizeof(desc_val), false);
}
