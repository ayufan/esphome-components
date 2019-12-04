#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef ARDUINO_ARCH_ESP32
#include <memory>

class ESP32BLEClient;

class EQ3Climate :
  public esphome::climate::Climate,
  public esphome::PollingComponent
{
public:
  EQ3Climate();
  ~EQ3Climate();

  void setup() override;
  void update() override;
  void update_retry(int retry);
  void control(const esphome::climate::ClimateCall &call) override;
  void control_retry(esphome::climate::ClimateCall call, int retry);
  void dump_config() override;
  esphome::climate::ClimateTraits traits();

  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_address(uint64_t new_address) { address = new_address; }
  void set_valve(esphome::sensor::Sensor *sensor) { valve = sensor; }
  void set_time(esphome::time::RealTimeClock *clock) { time_clock = clock; }

private:
  bool with_connection(std::function<bool()> handler);
  bool connect();
  void disconnect();
  bool send_command(uint8_t *command, uint16_t length);
  bool wait_for_notify(int timeout_ms = 3000);
  void reset_state();

private:
  bool query_id();
  bool query_state();
  bool query_schedule(uint8_t day);
  bool set_auto_mode();
  bool set_manual_mode();
  bool set_boost_mode(bool enabled);
  // bool set_away_mode()
  bool set_temperature(int temperature);
  bool set_temperature_type(int eco);
  bool set_temperature_offset(float offset);
  bool set_temperature_presets(int comfort, int eco);
  bool set_window_config(int seconds, int temperature);
  bool set_locked(bool locked);
  // bool set_away(int temperature, int till_when)
  // bool set_mode(int mode)

private:
  void parse_client_notify(const std::string &data);
  void parse_state(const std::string &data);
  void parse_schedule(const std::string &data);
  void parse_id(const std::string &data);

  uint64_t address{0};
  esphome::sensor::Sensor *valve{nullptr};
  esphome::time::RealTimeClock *time_clock{nullptr};
  std::unique_ptr<ESP32BLEClient> ble_client;
};

#endif
