#include "eq3.h"
#include "eq3_const.h"
#include "esphome/core/log.h"

#include "../esp32_ble_clients/esp32_ble_client.h"
#include "../esp32_ble_clients/esp32_ble.h"

using namespace esphome;
using namespace esphome::climate;

static const char *TAG = "eq3";

EQ3Climate::EQ3Climate() {
}

EQ3Climate::~EQ3Climate() {
}

void EQ3Climate::setup() {
  // ESP32BLE::instance().ble_setup();

  auto restore = restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }
}

void EQ3Climate::update() {
  cancel_timeout("update_retry");
  update_retry(3);
}

void EQ3Climate::update_retry(int tries) {
  ESP_LOGI(TAG, "Requesting update of %10llx...", address);

  bool success = with_connection([this]() {
    return query_state();
  });

  if (success) {
    ESP_LOGI(TAG, "Update of %10llx succeeded.", address);
  } else if (--tries > 0) {
    ESP_LOGW(TAG, "Update of %10llx failed. Tries left: %d.", address, tries);
    set_timeout("update_retry", 3000, [this, tries]() {
      update_retry(tries);
    });
  } else {
    ESP_LOGW(TAG, "Update of %10llx failed. Too many tries.", address);
    reset_state();
  }
}

void EQ3Climate::reset_state() {
  if (valve) {
    valve->publish_state(NAN);
  }
}

void EQ3Climate::control(const ClimateCall &call) {
  cancel_timeout("control_retry");
  ClimateCall call_data = call;
  defer("control_retry", [this, call]() {
    control_retry(call, 0);
  });
}

void EQ3Climate::control_retry(ClimateCall call, int tries) {
  ESP_LOGI(TAG, "Requesting climate control of %10llx...", address);

  bool success = with_connection([this, call]() {
    int calls = 0;

    if (call.get_target_temperature().has_value()) {
      calls += set_temperature(*call.get_target_temperature());
    }

    if (call.get_mode().has_value()) {
      switch (*call.get_mode()) {
      default:
      case CLIMATE_MODE_AUTO:
        calls += set_auto_mode();
        break;
      
      case CLIMATE_MODE_HEAT:
        calls += set_manual_mode();
        break;
      
      case CLIMATE_MODE_OFF:
        calls += set_temperature(EQ3BT_OFF_TEMP);
        break;
      }
    }

    if (!calls) {
      calls += query_state();
    }

    return calls > 0;
  });

  if (success) {
    ESP_LOGW(TAG, "Climate control of %10llx succeeded.", address);
  } else if (--tries > 0) {
    ESP_LOGW(TAG, "Climate control of %10llx failed. Tries left: %d.", address, tries);
    set_timeout("control_retry", 3000, [this, call, tries]() {
      control_retry(call, tries);
    });
  } else {
    ESP_LOGW(TAG, "Climate control of %10llx failed. Too many tries.", address);
  }
}

void EQ3Climate::parse_state(const std::string &data) {
  if (data.size() < sizeof(DeviceState)) {
    return;
  }

  auto state = (DeviceState*)data.c_str();
  target_temperature = state->target_temp / 2.0f;

  if (state->mode.boost_mode) {
    target_temperature = EQ3BT_ON_TEMP;
    mode = CLIMATE_MODE_HEAT;
  } else if (target_temperature == EQ3BT_OFF_TEMP) {
    mode = CLIMATE_MODE_OFF;
  } else if (state->mode.manual_mode) {
    mode = CLIMATE_MODE_HEAT;
  } else {
    mode = CLIMATE_MODE_AUTO;
  }

  away = state->mode.away_mode;

  if (valve) {
    valve->publish_state(state->valve);
  }

  publish_state();
}

void EQ3Climate::parse_schedule(const std::string &data) {
}

void EQ3Climate::parse_id(const std::string &data) {
}

ClimateTraits EQ3Climate::traits() {
  auto traits = ClimateTraits();
  traits.set_supports_auto_mode(true);
  traits.set_supports_heat_mode(true);
  traits.set_supports_away(false); // currently not working
  traits.set_visual_min_temperature(EQ3BT_MIN_TEMP);
  traits.set_visual_max_temperature(EQ3BT_MAX_TEMP);
  traits.set_visual_temperature_step(0.5);
  return traits;
}

void EQ3Climate::dump_config() {
  LOG_CLIMATE("", "EQ3-Max Thermostat", this);
  ESP_LOGCONFIG(TAG, "  Mac Address: %10llx", this->address);
  LOG_SENSOR("  ", "Valve", valve);
}
