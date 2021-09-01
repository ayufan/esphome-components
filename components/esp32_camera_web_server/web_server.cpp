#ifdef ARDUINO_ARCH_ESP32

#include "web_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

#include <cstdlib>
#include <ESPAsyncWebServer.h>

#ifdef USE_LOGGER
#include <esphome/components/logger/logger.h>
#endif

namespace esphome {
namespace esp32_camera_web_server {

static const int IMAGE_REQUEST_TIMEOUT = 1000;
static const char *TAG = "esp32_camera_web_server";

WebServer::WebServer(web_server_base::WebServerBase *base) : base_(base) {
}

WebServer::~WebServer() {
}

void WebServer::setup() {
  this->base_->init();
  this->base_->add_handler(this);

  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->add_image_callback([this](std::shared_ptr<esp32_camera::CameraImage> image) {
      if (this->last_image_) {
        return;
      }

      if (this->last_requested_) {
        this->last_image_ = image;
      }
    });
  }
}

bool WebServer::canHandle(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    if (request->url() == "/snapshot")
      return true;
  }

  return false;
}

void WebServer::handleRequest(AsyncWebServerRequest *request) {
  if (request->url() == "/snapshot")
    this->handle_snapshot(request);
  else if (request->url() == "/stream")
    this->handle_stream(request);
}

void WebServer::handle_snapshot(AsyncWebServerRequest *request) {
  this->last_image_ = nullptr;

  if (!esphome::esp32_camera::global_esp32_camera) {
    request->send(404);
    return;
  }

  esphome::esp32_camera::global_esp32_camera->request_image();
  this->last_requested_ = millis();

  request->send("image/jpeg", 0, [&](uint8_t *buffer, size_t maxLen, size_t offset) -> size_t {
    if (!this->last_image_) {
      if (millis() - this->last_requested_ > IMAGE_REQUEST_TIMEOUT) {
        this->last_requested_ = 0;
        return 0;
      }

      ESP_LOGD(TAG, "No image yet");

      return RESPONSE_TRY_AGAIN;
    }

    size_t length = min(offset + min(maxLen, (size_t)1024), this->last_image_->get_data_length());
    size_t n = length - offset;
    memcpy(buffer, this->last_image_->get_data_buffer() + offset, n);

    ESP_LOGD(TAG, "Sending offset:%d, size:%d, n:%d", offset, length, n);

    // clear image
    if(length == this->last_image_->get_data_length()) {
      this->last_requested_ = 0;
      this->last_image_ = nullptr;
    }
    return n;
  });
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void WebServer::handle_stream(AsyncWebServerRequest *request) {
  this->last_image_ = nullptr;

  if (!esphome::esp32_camera::global_esp32_camera) {
    request->send(404);
    return;
  }

  esphome::esp32_camera::global_esp32_camera->request_stream();
  this->last_requested_ = millis();

  request->send("image/jpeg", 0, [&](uint8_t *buffer, size_t maxLen, size_t offset) -> size_t {
    if (!this->last_image_) {
      if (millis() - this->last_requested_ > IMAGE_REQUEST_TIMEOUT) {
        this->last_requested_ = 0;
        return 0;
      }

      ESP_LOGD(TAG, "No image yet");

      return RESPONSE_TRY_AGAIN;
    }

    size_t length = min(offset + min(maxLen, (size_t)1024), this->last_image_->get_data_length());
    size_t n = length - offset;
    memcpy(buffer, this->last_image_->get_data_buffer() + offset, n);

    ESP_LOGD(TAG, "Sending offset:%d, size:%d, n:%d", offset, length, n);

    // clear image
    if(length == this->last_image_->get_data_length()) {
      this->last_requested_ = millis();
      this->last_image_ = nullptr;
      this->last_send_ = 0;
    }
    return n;
  });
}

void WebServer::dump_config() {
}

float WebServer::get_setup_priority() const {
  return setup_priority::LATE;
}

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif