#include "cometblue.h"
#include "cometblue_const.h"
#include "esphome/core/log.h"

#include "../esp32_ble_clients/esp32_ble_client.h"
#include "../esp32_ble_clients/esp32_ble.h"

using namespace esphome;
using namespace esphome::climate;

static const char *TAG = "cometblue";

CometBlueClimate::CometBlueClimate() {
}

CometBlueClimate::~CometBlueClimate() {
}

void CometBlueClimate::setup() {
  auto restore = restore_state_();
  if (restore.has_value()) {
    restore->apply(this);    
  } 
}


void CometBlueClimate::update() {
  cancel_timeout("update_retry");

  update_retry(3);
}

void CometBlueClimate::update_retry(int tries) {
  ESP_LOGI(TAG, "Requesting update of %10llx...", address);

  bool success = with_connection([this]() {
    return send_pincode() && query_state();
  });

  if (success) {
    publish_state();
  }

  if (success) {
    ESP_LOGI(TAG, "Update of %10llx succeeded.", address);
  } else if (--tries > 0) {
    ESP_LOGW(TAG, "Update of %10llx failed. Tries left: %d.", address, tries);
    set_timeout("update_retry", 10000, [this, tries]() {
      update_retry(tries);
    });
  } else {
    ESP_LOGW(TAG, "Update of %10llx failed. Too many tries.", address);
    reset_state();
  }
}

void CometBlueClimate::reset_state() {
}

void CometBlueClimate::control(const ClimateCall &call) {
  cancel_timeout("control_retry");
  ClimateCall call_data = call;
  defer("control_retry", [this, call]() {
    control_retry(call, 3);
  });
}

void CometBlueClimate::control_retry(ClimateCall call, int tries) {
  ESP_LOGI(TAG, "Requesting climate control of %10llx...", address);

  bool success = with_connection([this, call]() {
    int calls = 0;

    calls += send_pincode();

    if (call.get_target_temperature().has_value()) {
      calls += set_temperature(*call.get_target_temperature());
    }

    if (call.get_mode().has_value()) {
      switch (*call.get_mode()) {
      default:      
      case CLIMATE_MODE_HEAT:
        calls += set_manual_mode();
        break;
      
      case CLIMATE_MODE_OFF:
        calls += set_off_mode();
        break;
      }
    }

    if (!calls) {
      calls += query_state();
    }

    if (!calls) {
      publish_state();
    }

    return calls > 0;
  });

  if (success) {
    ESP_LOGW(TAG, "Climate control of %10llx succeeded.", address);
  } else if (--tries > 0) {
    ESP_LOGW(TAG, "Climate control of %10llx failed. Tries left: %d.", address, tries);
    set_timeout("control_retry", 10000, [this, call, tries]() {
      control_retry(call, tries);
    });
  } else {
    ESP_LOGW(TAG, "Climate control of %10llx failed. Too many tries.", address);
  }
}



ClimateTraits CometBlueClimate::traits() {
  auto traits = ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_heat_mode(true);
  traits.set_visual_min_temperature(COMETBLUEBT_MIN_TEMP);
  traits.set_visual_max_temperature(COMETBLUEBT_MAX_TEMP);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void CometBlueClimate::dump_config() {
  LOG_CLIMATE("", "CometBlue Thermostat", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Mac Address: %10llx", this->address);
}
