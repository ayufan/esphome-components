#include "eq3.h"
#include "eq3_const.h"
#include "esphome/core/log.h"

#include "../esp32_ble_clients/esp32_ble.h"
#include "../esp32_ble_clients/esp32_ble_client.h"

using namespace esphome;

static const char *TAG = "eq3_cmd";

static uint8_t temp_to_dev(const float &value) {
  if (value < EQ3BT_MIN_TEMP)
    return uint8_t(EQ3BT_MIN_TEMP * 2);
  else if (value > EQ3BT_MAX_TEMP)
    return uint8_t(EQ3BT_MAX_TEMP * 2);
  else
    return uint8_t(value * 2);
}

bool EQ3Climate::with_connection(std::function<bool()> handler) {
  bool success = true;

  success = success && connect();
  success = success && handler();
  success = success && wait_for_notify();
  disconnect();

  return success;
}

bool EQ3Climate::connect() {
  if (ble_client && ble_client->is_connected()) {
    return true;
  }

  std::unique_ptr<ESP32BLEClient> new_ble_client;

  new_ble_client.swap(ble_client);
  new_ble_client.reset(ESP32BLE::instance().acquire());

  if (!new_ble_client) {
    ESP_LOGW(TAG, "Cannot acquire client for %10llx.", address);
    return false;
  }

  new_ble_client->set_address(address);

  ESP_LOGV(TAG, "Connecting to %10llx...", address);
  
  if (!new_ble_client->connect()) {
    ESP_LOGW(TAG, "Cannot connect to %10llx.", address);
    return false;
  }

  if (!new_ble_client->request_services()) {
    new_ble_client.reset();
    ESP_LOGW(TAG, "Cannot request services from %10llx.", address);
    return false;
  }

  uint16_t notify_handle = new_ble_client->get_characteristic(
    PROP_SERVICE_UUID, PROP_NOTIFY_CHARACTERISTIC_UUID);

  if (!notify_handle) {
    new_ble_client.reset();
    ESP_LOGE(TAG, "Cannot find notification handle for %10llx.", address);
    return false;
  }

  new_ble_client->register_notify(notify_handle, true);
  new_ble_client->write_notify_desc(notify_handle, true, true);

  ESP_LOGV(TAG, "Connected to %10llx.", address);
  new_ble_client.swap(ble_client);
  return true;
}

void EQ3Climate::disconnect() {
  if (!ble_client) {
    return;
  }

  ble_client.reset();
  ESP_LOGV(TAG, "Disconnected from %10llx.", address);
}

bool EQ3Climate::send_command(void *command, uint16_t length) {
  if (!ble_client) {
    return false;
  }

  uint16_t command_handle = ble_client->get_characteristic(
    PROP_SERVICE_UUID, PROP_COMMAND_CHARACTERISTIC_UUID);
  if (!command_handle) {
    ESP_LOGE(TAG, "Cannot find command handle for %10llx.", address);
    return false;
  }

  bool result = ble_client->write(
    ESP32BLEClient::Characteristic,
    command_handle,
    command, length,
    true);

  if (result) {
    ESP_LOGV(TAG, "Sent of `%s` to %10llx to handle %04x.",
      hexencode((const uint8_t*)command, length).c_str(), address, command_handle);
  } else {
    ESP_LOGV(TAG, "Send of `%s` to %10llx to handle %04x: %d",
      hexencode((const uint8_t*)command, length).c_str(), address, command_handle, result);
  }

  return result;
}

bool EQ3Climate::wait_for_notify(int timeout_ms) {
  if (!ble_client) {
    return false;
  }

  auto notifications = ble_client->wait_for_notifications(timeout_ms);

  for (auto iter = notifications.begin(); iter != notifications.end(); ++iter) {
    parse_client_notify(iter->data);
  }

  if (notifications.empty()) {
    ESP_LOGV(TAG, "Did not receive notification for %10llx.", address);
    return false;
  }

  ESP_LOGV(TAG, "Received notification for %10llx.", address);
  return true;
}

bool EQ3Climate::query_id() {
  uint8_t command[] = {PROP_ID_QUERY};
  return send_command(command, sizeof(command));
}

bool EQ3Climate::query_state() {
  if (!time_clock || !time_clock->now().is_valid()) {
    ESP_LOGE(TAG, "Clock source for %10llx is not valid.", address);
    return false;
  }

  auto now = time_clock->now();

  uint8_t command[] = {
    PROP_INFO_QUERY,
    uint8_t(now.year % 100),
    now.month,
    now.day_of_month,
    now.hour,
    now.minute,
    now.second
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::query_schedule(EQ3Day day) {
  if (day > EQ3_LastDay) {
    return false;
  }

  uint8_t command[] = {PROP_SCHEDULE_QUERY, day};
  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature(float temperature) {
  uint8_t command[2];

  if (temperature <= EQ3BT_OFF_TEMP) {
    command[0] = PROP_MODE_WRITE;
    command[1] = uint8_t(EQ3BT_OFF_TEMP * 2) | 0x40;
  } else if (temperature >= EQ3BT_ON_TEMP) {
    command[0] = PROP_MODE_WRITE;
    command[1] = uint8_t(EQ3BT_ON_TEMP * 2) | 0x40;
  } else {
    command[0] = PROP_TEMPERATURE_WRITE;
    command[1] = uint8_t(temperature * 2);
  }

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_auto_mode() {
  uint8_t command[] = {
    PROP_MODE_WRITE,
    0
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_manual_mode() {
  uint8_t command[] = {
    PROP_MODE_WRITE,
    0x40
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_boost_mode(bool enabled) {
  uint8_t command[] = {
    PROP_BOOST,
    uint8_t(enabled ? 1 : 0)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_offset(float offset) {
  // [-3,5 .. 0  .. 3,5 ]
  // [00   .. 07 .. 0e ]

  if (offset < -3.5 || offset > 3.5) {
    return false;
  }

  uint8_t command[] = {
    PROP_OFFSET,
    uint8_t((offset + 3.5) * 2)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_type(int eco) {
  uint8_t command[] = {
    eco ? PROP_ECO : PROP_COMFORT
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_presets(float comfort, float eco) {
  uint8_t command[] = {
    PROP_COMFORT_ECO_CONFIG,
    temp_to_dev(comfort),
    temp_to_dev(eco)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_locked(bool locked) {
  uint8_t command[] = {
    PROP_LOCK,
    uint8_t(locked ? 1 : 0)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_window_config(int seconds, float temperature) {
  if (seconds < 0 || seconds > 3600) {
    return false;
  }

  uint8_t command[] = {
    PROP_WINDOW_OPEN_CONFIG,
    temp_to_dev(temperature),
    uint8_t(seconds / 300)
  };

  return send_command(command, sizeof(command));
}

void EQ3Climate::parse_client_notify(std::string data) {
  if (data.size() >= 2 && data[0] == PROP_INFO_RETURN && data[1] == 0x1) {
    defer("parse_notify", [this, data]() {
      parse_state(data);
    });
  } else if (data.size() >= 1 && data[0] == PROP_SCHEDULE_RETURN) {
    defer("parse_schedule_" + to_string(data[1]), [this, data]() {
      parse_schedule(data);
    });
  } else if (data.size() >= 1 && data[0] == PROP_ID_RETURN) {
    defer("parse_id", [this, data]() {
      parse_id(data);
    });
  } else {
    ESP_LOGW(TAG, "Received unknown characteristic from %10llx: %s.",
      address,
      hexencode((const uint8_t*)data.c_str(), data.size()).c_str());
  }
}
