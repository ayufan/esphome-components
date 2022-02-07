#include "tplink_component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

namespace esphome {
namespace tplink {

static const char *TAG = "tplink_plug";
static const int PORT = 9999;
static const int MAX_CLIENTS = 2;
static const int MAX_PACKET_SIZE = 100;

struct Client {
  WiFiClient socket;
  int length;
};

TplinkComponent::TplinkComponent() {}

TplinkComponent::~TplinkComponent() {
  if (this->udp_) {
    this->udp_->stop();
  }
  if (this->tcp_) {
    this->tcp_->stop();
  }
}

void TplinkComponent::setup() {
  this->udp_.reset(new WiFiUDP());

  if (!this->udp_->begin(PORT)) {
    ESP_LOGE(TAG, "Cannot bind UDP to %d.", PORT);
    mark_failed();
  }

  this->tcp_.reset(new WiFiServer(PORT));
  this->tcp_->setNoDelay(false);
  this->tcp_->begin(PORT, 1);
}

void TplinkComponent::loop() {
  this->loop_udp_();
  this->loop_tcp_accept_();
  this->loop_tcp_();
}

void TplinkComponent::loop_tcp_accept_() {
  if (auto new_socket = this->tcp_->available()) {
    if (this->clients_.size() < MAX_CLIENTS) {
      new_socket.setTimeout(3);
  
      this->clients_.push_back(
        new Client{.socket = new_socket, .length = -1}
      );

      ESP_LOGV(TAG, "TCP accepted: %s:%d. Count: %d",
        new_socket.remoteIP().toString().c_str(),
        new_socket.remotePort(), this->clients_.size());
    } else {
      ESP_LOGW(TAG, "TCP ignored: %s:%d. Too many connections",
        new_socket.remoteIP().toString().c_str(),
        new_socket.remotePort());

      new_socket.stop();
    }
  }
}

void TplinkComponent::loop_tcp_() {
  int done = 0;

  for (int i = 0; i < this->clients_.size(); ++i) {
    auto client = this->clients_[i];

    if (client->socket && this->recv_tcp_(*client)) {
      this->clients_[done++] = client;
    } else {
      ESP_LOGV(TAG, "TCP done: %s:%d",
        client->socket.remoteIP().toString().c_str(),
        client->socket.remotePort());

      client->socket.stop();
      delete client;
    }
  }

  if (done < this->clients_.size()) {
    this->clients_.erase(this->clients_.begin() + done);
  }
}

void TplinkComponent::loop_udp_() {
  std::string payload;

  while (uint16_t packet_size = this->udp_->parsePacket()) {
    if (packet_size > MAX_PACKET_SIZE) {
      ESP_LOGW(TAG, "UDP packet out of bounds: %s:%d. %d bytes.",
        this->udp_->remoteIP().toString().c_str(),
        this->udp_->remotePort(),
        packet_size
      );
      this->udp_->flush();
      continue;
    }

    payload.resize(packet_size);

    if (!this->udp_->read((unsigned char*)&payload[0], payload.size())) {
      ESP_LOGW(TAG, "Failed to read UDP packet: %s:%d. %d bytes.",
        this->udp_->remoteIP().toString().c_str(),
        this->udp_->remotePort(),
        packet_size
      );
      this->udp_->flush();
      continue;
    }

    // clear read buffer
    this->udp_->flush();

    ESP_LOGV(TAG, "UDP received: %s:%d. %d bytes.",
      this->udp_->remoteIP().toString().c_str(),
      this->udp_->remotePort(),
      packet_size
    );

    this->process_([this](const std::string &output) {
      if (!this->udp_->beginPacket(this->udp_->remoteIP(), this->udp_->remotePort()))
        return false;
      
      if (!this->udp_->write((const uint8_t*)output.c_str(), output.size()))
        return false;

      return this->udp_->endPacket() != 0;
    }, payload);
  }
}

bool TplinkComponent::recv_tcp_(Client &client) {
  // read header
  if (client.length < 0) {
    // still valid, but not enough data for length
    if (client.socket.available() < sizeof(client.length)) {
      return true;
    }

    int ret = client.socket.read((uint8_t*)&client.length, sizeof(client.length));
    if (ret != sizeof(client.length)) {
      ESP_LOGW(TAG, "TCP packet header read length: %d, expected: %d.", ret, sizeof(client.length));
      return false;
    }

    client.length = htonl(client.length);

    if (client.length < 0 || client.length > MAX_PACKET_SIZE) {
      ESP_LOGW(TAG, "TCP packet out of bounds: %d.", client.length);
      return false;
    }
  }

  // still valid, but not enough data for length
  if (client.socket.available() < client.length) {
    return true;
  }

  std::string payload;
  payload.resize(client.length);

  int ret = client.socket.read((uint8_t*)&payload[0], payload.size());
  if (ret != payload.size()) {
    ESP_LOGW(TAG, "TCP packet read length: %d, expected: %d.", ret, payload.size());
    return false;
  }

  this->process_([&client](const std::string &output) {
    unsigned int size = ntohl(output.size());
    client.socket.write((const char*)&size, sizeof(size));
    return client.socket.write(output.c_str(), output.size());
  }, payload);

  // everything processed stop
  return false;
}

void TplinkComponent::process_(response_t response, std::string &s) {
  bool get_realtime = false;
  bool set_relay_state = false;
  bool set_led_state = false;
  bool requested_state = false;
  bool get_sysinfo = false;

  this->decrypt_(s);

  ESP_LOGV(TAG, "Received raw: %s", hexencode((const uint8_t*)s.c_str(), s.size()).c_str());

  esphome::json::parse_json(s, [&](JsonObject root) {
    ESP_LOGV(TAG, "Received valid JSON: %s.", s.c_str());

    if (!root["emeter"]["get_realtime"].isNull()) {
      get_realtime = true;
    } else if (!root["system"]["set_relay_state"].isNull()) {
      set_relay_state = true;
      requested_state = root["system"]["set_relay_state"]["state"] > 0;
    } else if (!root["system"]["get_sysinfo"].isNull()) {
      get_sysinfo = true;
    } else if (!root["system"]["set_led_state"].isNull()) {
      set_led_state = true;
      requested_state = root["system"]["set_led_state"]["off"] == 0;
    }
  });
  
  if (get_realtime) {
    this->process_get_realtime_(response);
  } else if (get_sysinfo) {
    this->process_get_sysinfo_(response);
  } else if (set_relay_state) {
    this->process_set_relay_state_(response, requested_state);
  } else if (set_led_state) {
    this->process_set_led_state_(response, requested_state);
  } else {
    ESP_LOGV(TAG, "Packet not understood.");
  }
}

void TplinkComponent::process_set_relay_state_(response_t response, bool requested_state) {
  for (auto &plug : this->plugs_) {
    if (requested_state) {
      plug.state_switch->turn_on();
    } else {
      plug.state_switch->turn_off();
    }

    this->send_json_(response, [](JsonObject root) {
      auto emeter = root.createNestedObject("system");
      auto relay_state = emeter.createNestedObject("set_relay_state");

      relay_state["err_code"] = 0;
    });
  }
}

void TplinkComponent::process_set_led_state_(response_t response, bool requested_state) {
}

void TplinkComponent::process_get_sysinfo_(response_t response) {
  this->send_json_(response, [this](JsonObject root) {
    auto system = root.createNestedObject("system");
    auto sysinfo = system.createNestedObject("get_sysinfo");
    sysinfo["err_code"] = 0;
    sysinfo["sw_ver"] = "1.2.5 Build 171213 Rel.101523";
    sysinfo["hw_ver"] = "1.0";
    sysinfo["type"] = "IOT.SMARTPLUGSWITCH";
    sysinfo["model"] = "HS110(EU)";
    sysinfo["mac"] = get_mac_address_pretty();
    sysinfo["deviceId"] = get_mac_address_pretty();
    sysinfo["fwId"] = "00000000000000000000000000000000";
    sysinfo["oemId"] = "00000000000000000000000000000000";
    sysinfo["alias"] = App.get_name();
    sysinfo["dev_name"] = "Wi-Fi Smart Plug With Energy Monitoring";

    bool has_voltage_current = false;
  
    if (this->plugs_.size() == 1) {
      auto &plug = this->plugs_[0];
      sysinfo["relay_state"] = plug.state_switch->state ? 1 : 0;
      sysinfo["on_time"] = plug.state_switch->state ? 1 : 0;
      // sysinfo["led_off"] = 0
      has_voltage_current = plug.voltage_sensor || plug.current_sensor;
    } else {
      auto children = sysinfo.createNestedArray("children");
      for (auto &plug : this->plugs_) {
        auto pluginfo = children.createNestedObject();
        pluginfo["relay_state"] = plug.state_switch->state ? 1 : 0;
        pluginfo["on_time"] = plug.state_switch->state ? 1 : 0;
        // sysinfo["led_off"] = 0
        has_voltage_current = has_voltage_current || plug.voltage_sensor || plug.current_sensor;
      }
    }

    if (has_voltage_current) {
      sysinfo["feature"] = "TIM:ENE";
    } else {
      sysinfo["feature"] = "TIM";
    }
  
    sysinfo["rssi"] = WiFi.RSSI();
    sysinfo["updating"] = 0;
  });
}

void TplinkComponent::process_get_realtime_(response_t response) {
  for (auto &plug : this->plugs_) {
    this->send_json_(response, [&plug](JsonObject root) {
      auto emeter = root.createNestedObject("emeter");
      auto realtime = emeter.createNestedObject("get_realtime");

      realtime["err_code"] = 0;

      if (plug.current_sensor && plug.current_sensor->has_state()) {
        realtime["current"] = plug.current_sensor->state;
      } else {
        realtime["err_code"] = 1;
      }

      if (plug.voltage_sensor && plug.voltage_sensor->has_state()) {
        realtime["voltage"] = plug.voltage_sensor->state;
      } else {
        realtime["err_code"] = 2;
      }

      if (plug.current_sensor && plug.current_sensor->has_state() && plug.voltage_sensor && plug.voltage_sensor->has_state()) {
        realtime["power"] = plug.current_sensor->state * plug.voltage_sensor->state;
      } else {
        realtime["err_code"] = 3;
      }

      if (plug.total_sensor && plug.total_sensor->has_state()) {
        realtime["total"] = plug.total_sensor->state / 1000.0f;
      }
    });
  }
}

bool TplinkComponent::send_json_(response_t response, const esphome::json::json_build_t &fn) {
  std::string output = esphome::json::build_json(fn);

  if (output.empty() || !response) {
    return false;
  }

  ESP_LOGV(TAG, "Sending back: %s", output.c_str());

  this->encrypt_(output);
  return response(output);
}

void TplinkComponent::decrypt_(std::string &s) {
  uint8_t key = 171;

  for (int i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    s[i] = c ^ key;
    key = c;
  }
}

void TplinkComponent::encrypt_(std::string &s) {
  uint8_t key = 171;

  for (int i = 0; i < s.length(); i++) {
    uint8_t d = s[i];
    key = s[i] = d ^ key;
  }
}

void TplinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TP-Link Plugs:");
  ESP_LOGCONFIG(TAG, "  Plugs: %d", this->plugs_.size());
}

}  // namespace tplink
}  // namespace esphome
