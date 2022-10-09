#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <WiFiServer.h>

#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace esp32_camera_web_server {

enum Mode { STREAM, SNAPSHOT };

class CameraWebServer : public Component {
 public:
  CameraWebServer();
  ~CameraWebServer();

  void setup() override;
  void on_shutdown() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_port(uint16_t port) { this->port_ = port; }
  void set_mode(Mode mode) { this->mode_ = mode; }

 protected:
  std::shared_ptr<esphome::esp32_camera::CameraImage> wait_for_image_();
  void server_loop_();
  void handler_();
  bool check_headers_();
  void streaming_handler_();
  void snapshot_handler_();
  void send_all(const char *buf, size_t buf_len);

 protected:
  uint16_t port_{0};
  WiFiServer server_;
  WiFiClient client_;
  SemaphoreHandle_t semaphore_;
  TaskHandle_t task_;
  std::shared_ptr<esphome::esp32_camera::CameraImage> image_;
  Mode mode_{STREAM};
};

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif  // USE_ESP32
