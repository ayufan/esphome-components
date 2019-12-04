#include "esp32_ble.h"
#include "esp32_ble_lock.h"
#include "esp32_ble_log.h"
#include "esp32_ble_client.h"

static const char *TAG = "esp32_ble_client_state";
static const int MAX_EVENTS = 100;

using namespace esphome;

bool ESP32BLEClient::set_state(State new_state) {
  if (state == new_state) {
    return true;
  }

  auto old_state = state;
  state = new_state;

  BLE_LOGD(TAG, "Changing state: %d => %d\n", old_state, state);
  return true;
}

void ESP32BLEClient::client_event_handler(esp_gattc_cb_event_t event, 
  esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param)
{
  ESP32BLELock lock(this->lock);

  this->gattc_if = gattc_if;

  auto handler = event_handlers[event];
  if (!handler) {
    BLE_LOGD(TAG, "ProcessingEvent: event=%d => is not handled.\n", event);
    return;
  }

  BLE_LOGD(TAG, "ProcessingEvent: event=%d...\n", event);

  std::unique_ptr<EventResult> result(new EventResult());
  result->type = event;
  if (param) {
    result->param = *param;
  }
  handler(*result);
}
