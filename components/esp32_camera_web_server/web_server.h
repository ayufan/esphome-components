#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/esp32_camera/esp32_camera.h"

namespace esphome {
namespace esp32_camera_web_server {

class WebServer : public Component, public AsyncWebHandler {
 public:
  WebServer(web_server_base::WebServerBase *base);
  ~WebServer();

  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void handle_snapshot(AsyncWebServerRequest *request);
  void handle_stream(AsyncWebServerRequest *request);

 protected:
  web_server_base::WebServerBase *base_{nullptr};
  std::shared_ptr<esphome::esp32_camera::CameraImage> last_image_;
  uint32_t last_requested_{0};
  uint32_t last_send_{0};
};

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif // ARDUINO_ARCH_ESP32
