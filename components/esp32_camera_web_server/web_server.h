#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/esp32_camera/esp32_camera.h"

struct httpd_req;

namespace esphome {
namespace esp32_camera_web_server {

class WebServer : public Component {
 public:
  WebServer();
  ~WebServer();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;
  void on_shutdown() override;

  void dump_config() override;

  float get_setup_priority() const override;

  void set_stream_port(uint16_t port) { this->stream_port_ = port; }
  void set_snapshot_port(uint16_t port) { this->snapshot_port_ = port; }

  void loop() override;

 private:
  static esp_err_t stream_handler_(struct httpd_req *req);
  static esp_err_t snapshot_handler_(struct httpd_req *req);

 protected:
  uint16_t stream_port_{0}, snapshot_port_{0};
  void *stream_httpd_{nullptr}, *snapshot_httpd_{nullptr};
  SemaphoreHandle_t stream_semaphore_, snapshot_semaphore_;
  std::shared_ptr<esphome::esp32_camera::CameraImage> stream_image_, snapshot_image_;
  bool streaming_{false}, snapshot_{false};
};

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif // ARDUINO_ARCH_ESP32
