#ifdef USE_ESP32

#include "camera_web_server.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cstdlib>
#include <esp_http_server.h>
#include <utility>

namespace esphome {
namespace esp32_camera_web_server {

static const int IMAGE_REQUEST_TIMEOUT = 5000;
static const char *const TAG = "esp32_camera_web_server";

#define PART_BOUNDARY "123456789000000000000987654321"
#define CONTENT_TYPE "image/jpeg"
#define CONTENT_LENGTH "Content-Length"

static const char *const STREAM_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\n"
                                         "\r\n"
                                         "--" PART_BOUNDARY "\r\n";
static const char *const SNAPSHOT_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: " CONTENT_TYPE "\r\n"
                                         "Content-Disposition: inline; filename=capture.jpg\r\n"
                                         "\r\n";
static const char *const NOT_FOUND_ERROR = "HTTP/1.0 404 Not Found\r\n\r\n";
static const char *const SERVICE_UNAVAILABLE = "HTTP/1.0 503 Service Unavailable\r\n\r\n";
static const char *const STREAM_PART = "Content-Type: " CONTENT_TYPE "\r\n" CONTENT_LENGTH ": %u\r\n\r\n";
static const char *const STREAM_BOUNDARY = "\r\n"
                                           "--" PART_BOUNDARY "\r\n";

CameraWebServer::CameraWebServer() {}

CameraWebServer::~CameraWebServer() {}

void CameraWebServer::setup() {
  if (!esp32_camera::global_esp32_camera || esp32_camera::global_esp32_camera->is_failed()) {
    this->mark_failed();
    return;
  }

  this->server_.begin(this->port_);
  this->server_.setTimeout(3);

  if (!this->server_) {
    this->mark_failed();
    return;
  }

  this->semaphore_ = xSemaphoreCreateBinary();
  
  xTaskCreateUniversal(
    [](void *handle) {
      ((CameraWebServer*)handle)->server_loop_();
    },
    "esp32_camera_web_server",
    4096,
    this,
    0,
    &this->task_,
    CONFIG_ARDUINO_RUNNING_CORE
  );

  esp32_camera::global_esp32_camera->add_image_callback([this](std::shared_ptr<esp32_camera::CameraImage> image) {
    if (this->client_ && image->was_requested_by(esp32_camera::WEB_REQUESTER)) {
      this->image_ = std::move(image);
      xSemaphoreGive(this->semaphore_);
    }
  });
}

void CameraWebServer::on_shutdown() {
  this->image_ = nullptr;
  this->client_.stop();
  this->server_.stop();
  if (this->task_) {
    vTaskDelete(this->task_);
    this->task_ = nullptr;
  }
  if (this->semaphore_) {
    vSemaphoreDelete(this->semaphore_);
    this->semaphore_ = nullptr;
  }
}

void CameraWebServer::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Camera Web Server:");
  ESP_LOGCONFIG(TAG, "  Port: %d", this->port_);
  if (this->mode_ == STREAM)
    ESP_LOGCONFIG(TAG, "  Mode: stream");
  else
    ESP_LOGCONFIG(TAG, "  Mode: snapshot");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup Failed");
  }
}

float CameraWebServer::get_setup_priority() const { return setup_priority::LATE; }

std::shared_ptr<esphome::esp32_camera::CameraImage> CameraWebServer::wait_for_image_() {
  std::shared_ptr<esphome::esp32_camera::CameraImage> image;
  image.swap(this->image_);

  if (!image) {
    // retry as we might still be fetching image
    xSemaphoreTake(this->semaphore_, IMAGE_REQUEST_TIMEOUT / portTICK_PERIOD_MS);
    image.swap(this->image_);
  }

  return image;
}

void CameraWebServer::server_loop_() {
  // This implements a very minimalistic a single-request web server
  while (this->server_) {
    yield();

    if (!this->server_.hasClient()) {
      continue;
    }

    this->client_ = this->server_.available();
    this->handler_();
    this->client_.stop();
    this->image_ = nullptr;
  }
}

void CameraWebServer::handler_() {
  if (!this->check_headers_()) {
    return;
  }

  switch (this->mode_) {
    case STREAM:
      this->streaming_handler_();
      break;

    case SNAPSHOT:
      this->snapshot_handler_();
      break;
  }
}

bool CameraWebServer::check_headers_() {
  char buf[20];

  size_t read = this->client_.readBytesUntil('\r', buf, sizeof(buf)-1);
  buf[read] = 0;

  if (strcmp(buf, "GET / HTTP/1.0") && strcmp(buf, "GET / HTTP/1.1")) {
    this->client_.write(NOT_FOUND_ERROR);
    return false;
  }

  return true;
}

void CameraWebServer::send_all(const char *buf, size_t buf_len) {
  int ret;

  while (this->client_ && buf_len > 0) {
    ret = this->client_.write(buf, buf_len);
    if (ret == 0) {
      this->client_.stop(); // send failed, stop
      return;
    }
    buf += ret;
    buf_len -= ret;
  }
}

void CameraWebServer::streaming_handler_() {
  // This manually constructs HTTP response to avoid chunked encoding
  // which is not supported by some clients

  char part_buf[64];
  uint32_t last_frame = millis();
  uint32_t frames = 0;

  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->start_stream(esphome::esp32_camera::WEB_REQUESTER);
  }

  while (this->client_) {
    auto image = this->wait_for_image_();

    if (!image) {
      ESP_LOGW(TAG, "STREAM: failed to acquire frame");
      break;
    }

    if (!frames) {
      this->send_all(STREAM_HEADER, strlen(STREAM_HEADER));
    }

    size_t hlen = snprintf(part_buf, 64, STREAM_PART, image->get_data_length());
    this->send_all(part_buf, hlen);
    this->send_all((const char *) image->get_data_buffer(), image->get_data_length());
    this->send_all(STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    frames++;
    int64_t frame_time = millis() - last_frame;
    last_frame = millis();

    ESP_LOGD(TAG, "MJPG: %uB %ums (%.1ffps)", (uint32_t) image->get_data_length(), (uint32_t) frame_time,
              1000.0 / (uint32_t) frame_time);
  }

  if (!frames) {
    this->send_all(SERVICE_UNAVAILABLE, strlen(SERVICE_UNAVAILABLE));
  }

  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->stop_stream(esphome::esp32_camera::WEB_REQUESTER);
  }

  ESP_LOGI(TAG, "STREAM: closed. Frames: %u", frames);
}

void CameraWebServer::snapshot_handler_() {
  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->request_image(esphome::esp32_camera::WEB_REQUESTER);
  }

  auto image = this->wait_for_image_();
  if (!image) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to acquire frame");
    this->send_all(SERVICE_UNAVAILABLE, strlen(SERVICE_UNAVAILABLE));
    return;
  }

  this->send_all(SNAPSHOT_HEADER, strlen(SNAPSHOT_HEADER));
  this->send_all((const char *) image->get_data_buffer(), image->get_data_length());
}

}  // namespace esp32_camera_web_server
}  // namespace esphome

#endif  // USE_ESP32
