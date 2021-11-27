#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/json/json_util.h"

#include <memory>
#include <set>
#include <map>

class UDP;
class WiFiServer;
class WiFiClient;

namespace esphome {
namespace tplink {

struct Plug {
  esphome::sensor::Sensor *current_sensor;
  esphome::sensor::Sensor *voltage_sensor;
  esphome::sensor::Sensor *total_sensor;
  esphome::switch_::Switch *state_switch;
};

struct Client;

class TplinkComponent : public esphome::Component {
 public:
  TplinkComponent();
  ~TplinkComponent();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void add_plug(const Plug &plug) { plugs_.push_back(plug); }

 private:
  void loop_udp_();
  void loop_tcp_();
  void loop_tcp_accept_();
  bool recv_tcp_(Client &client);
  void process_(Stream *response, std::string &s);
  void process_get_realtime_(Stream *response);
  void process_get_sysinfo_(Stream *response);
  void process_set_relay_state_(Stream *response, bool requested_state);
  void process_set_led_state_(Stream *response, bool requested_state);
  bool send_json_(Stream *response, const esphome::json::json_build_t &fn);
  void decrypt_(std::string &s);
  void encrypt_(std::string &s);

 protected:
  std::vector<Plug> plugs_;

  std::unique_ptr<UDP> udp_;
  std::unique_ptr<WiFiServer> tcp_;
  std::vector<Client*> clients_;
};

}  // namespace e131
}  // namespace esphome
