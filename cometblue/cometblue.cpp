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
    this->publish_state();
  } else {
    set_off_mode();
    this->publish_state();
  }
}

// Read all data and update state
void CometBlueClimate::update() {
  bool success = with_connection([this]() {
    return query_state();
  });

  this->publish_state();

}

void CometBlueClimate::reset_state() {
}

void CometBlueClimate::control(const ClimateCall &call) {
  ESP_LOGV(TAG, "Control call.");
  bool success = with_connection([this, call]() {
    send_pincode();
   
    if (call.get_target_temperature().has_value()) {
      auto temperature = *call.get_target_temperature();
      set_temperature(temperature);
    }

    if (call.get_mode().has_value()) {
      auto mode = *call.get_mode();

      switch (mode) {
      default:
      case CLIMATE_MODE_AUTO:
        set_auto_mode();
        break;
      
      case CLIMATE_MODE_HEAT:
        set_manual_mode();
        break;
      
      case CLIMATE_MODE_OFF:
        set_off_mode();
        break;
      }      
    }

    // Publish updated state   
    this->publish_state();

    return true;
  });
  
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
