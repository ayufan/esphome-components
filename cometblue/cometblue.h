#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/time/real_time_clock.h"


#ifdef ARDUINO_ARCH_ESP32
#include <memory>

#include <esp_bt_defs.h>

class ESP32BLEClient;

enum CometBlueDay
{
  CometBlue_FirstDay = 0,
  CometBlue_Saturday = CometBlue_FirstDay,
  CometBlue_Sunday,
  CometBlue_Monday,
  CometBlue_Tuesday,
  CometBlue_Wednesday,
  CometBlue_Thursday,
  CometBlue_Friday,
  CometBlue_LastDay
};

class CometBlueClimate :
  public esphome::climate::Climate,
  public esphome::PollingComponent
{
public:
  CometBlueClimate();
  ~CometBlueClimate();

public:
  void setup() override;
  void dump_config() override;
  esphome::climate::ClimateTraits traits();
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_address(uint64_t new_address) { address = new_address; }
  void set_pin(uint32_t new_pin) { pin = new_pin; }
  void set_temperature_offset(float offset) { temperature_offset = offset; }
  void set_time(esphome::time::RealTimeClock *clock) { time_clock = clock; }
  void set_window_open_config(int8_t sensitivity, int8_t minutes) { window_open_sensitivity = sensitivity; window_open_minutes = minutes; }

public:
  void control(const esphome::climate::ClimateCall &call) override;
  void update() override;

private:  
  bool with_connection(std::function<bool()> handler);
  bool connect();
  void disconnect();
  bool write_value(void *command, uint16_t length, esp_bt_uuid_t uuid);
  bool read_value(esp_bt_uuid_t uuid);
  void reset_state();
  void update_retry(int retry);
  void control_retry(esphome::climate::ClimateCall call, int retry);
  
  bool send_pincode();
  bool get_time();
  bool get_temperature();
  bool get_flags();

private:  
  bool query_state();
  bool set_auto_mode();
  bool set_manual_mode();
  bool set_off_mode();
  bool set_temperature(float temperature);

private:
  void parse_flags(const uint8_t *data);
  void parse_temperature(const uint8_t *data);
  
  uint64_t address{0};
  uint32_t pin;
  float temperature_offset;
  uint8_t window_open_minutes;
  uint8_t window_open_sensitivity;
  esphome::time::RealTimeClock *time_clock{nullptr};
  std::unique_ptr<ESP32BLEClient> ble_client;  
};

#endif
