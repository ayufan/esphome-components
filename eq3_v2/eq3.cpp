#include "eq3.h"
#include "eq3_const.h"
#include "esphome/core/log.h"

#include "../esp32_ble_clients/esp32_ble_client.h"
#include "../esp32_ble_clients/esp32_ble.h"

using namespace esphome;
using namespace esphome::climate;

static const char *TAG = "eq3";

static const char *DAY_NAMES[EQ3_LastDay] = {
  "Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"
};

EQ3Climate::EQ3Climate() {
}

EQ3Climate::~EQ3Climate() {
}

void EQ3Climate::setup() {
  auto restore = restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }

  if (this->temperature_sensor) {
    this->temperature_sensor->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      // current temperature changed, publish state
      this->publish_state();
    });
  }

}

void EQ3Climate::update() {
  cancel_timeout("update_retry");
  if (!last_id.has_value()) {
    update_id();
  }
  if (!last_schedule[0].has_value()) {
    update_schedule();
  }
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

void EQ3Climate::update_id() {
  ESP_LOGI(TAG, "Requesting ID of %10llx...", address);

  bool success = with_connection([this]() {
    return query_id();
  });

  if (success) {
    ESP_LOGI(TAG, "ID of %10llx succeeded.", address);
  } else {
    ESP_LOGW(TAG, "ID of %10llx failed. Too many tries.", address);
  }
}

void EQ3Climate::update_schedule() {
  ESP_LOGI(TAG, "Requesting Schedule of %10llx...", address);

  bool success = with_connection([this]() {
    bool success = false;
    for (int day = EQ3_FirstDay; day < EQ3_LastDay; ++day) {
      if (!last_schedule[day].has_value()) {
        success = query_schedule((EQ3Day)day) || success;
      }
    }
    return success;
  });

  if (success) {
    ESP_LOGI(TAG, "Schedule of %10llx succeeded.", address);
  } else {
    ESP_LOGW(TAG, "Schedule of %10llx failed. Too many tries.", address);
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
    control_retry(call, 3);
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
  if (data.size() != sizeof(DeviceStateReturn)) {
    ESP_LOGW(TAG, "State parse of %10llx failed: %s.",
      address,
      hexencode((const uint8_t*)data.c_str(), data.size()).c_str());
    return;
  }

  auto state = (DeviceStateReturn*)data.c_str();

  ESP_LOGI(TAG, "'%s': Valve: %d%%. Target: %.1f. Mode: %s.",
    get_name().c_str(),
    state->valve,
    state->target_temp.to_user(),
    state->mode.to_string().c_str());

  ESP_LOGI(TAG, "'%s': Window Open: %d minutes. Temp: %.1f.",
    get_name().c_str(),
    state->window_open_time.to_minutes(),
    state->window_open_temp.to_user());

  ESP_LOGI(TAG, "'%s': Temp: Comfort: %.1f. Eco: %.1f. Offset: %.1f.",
    get_name().c_str(),
    state->comfort_temp.to_user(),
    state->eco_temp.to_user(),
    state->temp_offset.to_user());

  if (state->away.valid()) {
    ESP_LOGI(TAG, "'%s': Away: %04d-%02d-%02d %02d:%02d.",
      get_name().c_str(),
      state->away.to_year(),
      state->away.to_month(),
      state->away.to_day(),
      state->away.to_hour(),
      state->away.to_minute());
  }

  target_temperature = state->target_temp;

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
  if (data.size() < sizeof(DeviceScheduleReturn)) {
    ESP_LOGW(TAG, "Schedule parse of %10llx failed: %s.",
      address,
      hexencode((const uint8_t*)data.c_str(), data.size()).c_str());
    return;
  }

  auto schedule = (const DeviceScheduleReturn*)data.c_str();
  if (schedule->day > EQ3_LastDay) {
    ESP_LOGI(TAG, "Schedule for %10llx returned invalid day: %d.",
      address, schedule->day);
    return;
  }

  last_schedule[schedule->day] = true;

  for (int hour = 0; hour < schedule->hours_count(data.size()); ++hour) {
    auto hour_schedule = schedule->hours[hour];

    if (!hour_schedule.valid()) {
      continue;
    }

    ESP_LOGI(TAG, "'%s': Day %s: Schedule %d: Temp: %.1f. Till: %02d:%02d",
      get_name().c_str(), DAY_NAMES[schedule->day], hour,
      hour_schedule.temp.to_user(),
      hour_schedule.till.to_hour(),
      hour_schedule.till.to_minute());
  }
}

void EQ3Climate::parse_id(const std::string &data) {
  if (data.size() != sizeof(DeviceIDReturn)) {
    ESP_LOGW(TAG, "ID parse of %10llx failed: %s.",
      address,
      hexencode((const uint8_t*)data.c_str(), data.size()).c_str());
    return;
  }

  auto id = (const DeviceIDReturn*)data.c_str();
  last_id = true;

  ESP_LOGI(TAG, "'%s': Version: %d", get_name().c_str(),
    id->version);
  ESP_LOGI(TAG, "'%s': Serial: %s", get_name().c_str(),
    hexencode(id->serial, sizeof(id->serial)).c_str());
}

ClimateTraits EQ3Climate::traits() {
  auto traits = ClimateTraits();
  traits.set_supports_auto_mode(true);
  if (this->temperature_sensor) {
    traits.set_supports_current_temperature(true);
  }
  traits.set_supports_heat_mode(true);
  traits.set_supports_away(false); // currently not working
  traits.set_visual_min_temperature(EQ3BT_MIN_TEMP);
  traits.set_visual_max_temperature(EQ3BT_MAX_TEMP);
  traits.set_visual_temperature_step(0.5);
  return traits;
}

void EQ3Climate::dump_config() {
  LOG_CLIMATE("", "EQ3-Max Thermostat", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Mac Address: %10llx", this->address);
  LOG_SENSOR("  ", "Valve", valve);
}
