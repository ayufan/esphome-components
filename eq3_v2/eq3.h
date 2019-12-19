#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef ARDUINO_ARCH_ESP32
#include <memory>

class ESP32BLEClient;

enum EQ3Day
{
  EQ3_FirstDay = 0,
  EQ3_Saturday = EQ3_FirstDay,
  EQ3_Sunday,
  EQ3_Monday,
  EQ3_Tuesday,
  EQ3_Wednesday,
  EQ3_Thursday,
  EQ3_Friday,
  EQ3_LastDay
};

class EQ3Climate :
  public esphome::climate::Climate,
  public esphome::PollingComponent
{
public:
  EQ3Climate();
  ~EQ3Climate();

public:
  void setup() override;
  void dump_config() override;
  esphome::climate::ClimateTraits traits();
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_address(uint64_t new_address) { address = new_address; }
  void set_valve(esphome::sensor::Sensor *sensor) { valve = sensor; }
  void set_time(esphome::time::RealTimeClock *clock) { time_clock = clock; }
  void set_temperature_sensor(esphome::sensor::Sensor *sensor) { temperature_sensor = sensor; };

public:
  void control(const esphome::climate::ClimateCall &call) override;
  void update() override;
  void update_id();
  void update_schedule();

private:
  bool with_connection(std::function<bool()> handler);
  bool connect();
  void disconnect();
  bool send_command(void *command, uint16_t length);
  bool wait_for_notify(int timeout_ms = 3000);
  void reset_state();
  void update_retry(int retry);
  void control_retry(esphome::climate::ClimateCall call, int retry);

private:
  bool query_id();
  bool query_state();
  bool query_schedule(EQ3Day day);
  bool set_auto_mode();
  bool set_manual_mode();
  bool set_boost_mode(bool enabled);
  // bool set_away_mode()
  bool set_temperature(float temperature);
  bool set_temperature_type(int eco);
  bool set_temperature_offset(float offset);
  bool set_temperature_presets(float comfort, float eco);
  bool set_window_config(int seconds, float temperature);
  bool set_locked(bool locked);
  // bool set_away(float temperature, int till_when)
  // bool set_mode(int mode)

private:
  void parse_client_notify(std::string data);
  void parse_state(const std::string &data);
  void parse_schedule(const std::string &data);
  void parse_id(const std::string &data);

  uint64_t address{0};
  esphome::sensor::Sensor *valve{nullptr};
  esphome::time::RealTimeClock *time_clock{nullptr};
  /// The sensor used for getting the current temperature
  esphome::sensor::Sensor *temperature_sensor{nullptr};
  std::unique_ptr<ESP32BLEClient> ble_client;

  esphome::optional<bool> last_schedule[EQ3_LastDay];
  esphome::optional<bool> last_id;
};

#endif
