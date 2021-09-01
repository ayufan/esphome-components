#ifdef ARDUINO_ARCH_ESP32

#include "web_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

#include <cstdlib>
#include <esp_http_server.h>

#ifdef USE_LOGGER
#include <esphome/components/logger/logger.h>
#endif

namespace esphome {
namespace esp32_camera_web_server {

static const int IMAGE_REQUEST_TIMEOUT = 500;
static const char *TAG = "esp32_camera_web_server2";

// esp32_camera::global_esp32_camera

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

WebServer::WebServer() {
}

WebServer::~WebServer() {
}

void WebServer::setup() {
  this->stream_semaphore_ = xSemaphoreCreateBinary();
  this->snapshot_semaphore_ = xSemaphoreCreateBinary();

  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->add_image_callback([this](std::shared_ptr<esp32_camera::CameraImage> image) {
      if (this->streaming_) {
        this->stream_image_ = image;
        xSemaphoreGive(this->stream_semaphore_);
      }

      if (this->snapshot_) {
        this->snapshot_image_ = image;
        xSemaphoreGive(this->snapshot_semaphore_);
      }
    });
  }

  if (this->stream_port_) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = this->stream_port_;
    config.ctrl_port = this->stream_port_;
    if (httpd_start(&this->stream_httpd_, &config) == ESP_OK) {
      httpd_uri_t uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = stream_handler_,
        .user_ctx  = this
      };
      httpd_register_uri_handler(this->stream_httpd_, &uri);
    } else {
      mark_failed();
    }
  }

  if (this->snapshot_port_) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = this->snapshot_port_;
    config.ctrl_port = this->snapshot_port_;
    if (httpd_start(&this->snapshot_httpd_, &config) == ESP_OK) {
      httpd_uri_t uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = snapshot_handler_,
        .user_ctx  = this
      };
      httpd_register_uri_handler(this->snapshot_httpd_, &uri);
    } else {
      mark_failed();
    }
  }
}

void WebServer::on_shutdown() {
  this->streaming_ = false;
  this->stream_image_ = nullptr;
  httpd_stop(this->stream_httpd_);
  this->stream_httpd_ = nullptr;

  this->snapshot_ = false;
  this->snapshot_image_ = nullptr;
  httpd_stop(this->snapshot_httpd_);
  this->snapshot_httpd_ = nullptr;
}

void WebServer::dump_config() {
}

float WebServer::get_setup_priority() const {
  return setup_priority::LATE;
}

void WebServer::loop() {
  if (!this->streaming_) {
    this->stream_image_ = nullptr;
  }
  
  if (!this->snapshot_) {
    this->snapshot_image_ = nullptr;
  }
}

esp_err_t WebServer::stream_handler_(struct httpd_req *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  auto web_server = (WebServer*)req->user_ctx;

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    ESP_LOGW(TAG, "STREAM: failed to set HTTP response type");
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  uint32_t last_frame = millis();
  uint32_t frames = 0;

  web_server->streaming_ = true;
  web_server->stream_image_ = nullptr;

  while (res == ESP_OK && web_server->streaming_) {
    if (esp32_camera::global_esp32_camera != nullptr) {
      esp32_camera::global_esp32_camera->request_stream();
    }

    std::shared_ptr<esphome::esp32_camera::CameraImage> image;
    image.swap(web_server->stream_image_);

    if (!image) {
      // retry as we might still be fetching image
      xSemaphoreTake(web_server->stream_semaphore_, IMAGE_REQUEST_TIMEOUT / portTICK_PERIOD_MS);
      image.swap(web_server->stream_image_);
    }
    if (!image) {
      ESP_LOGW(TAG, "STREAM: failed to acquire frame");
      res = ESP_FAIL;
    }
    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
        size_t hlen = snprintf(part_buf, 64, _STREAM_PART, image->get_data_length());
        res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)image->get_data_buffer(), image->get_data_length());
    }
    if (res == ESP_OK) {
      frames++;
      int64_t frame_time = millis() - last_frame;
      last_frame = millis();

      ESP_LOGD(TAG, "MJPG: %uB %ums (%.1ffps)",
          (uint32_t)image->get_data_length(),
          (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }
  }

  if (!frames) {
    res = httpd_resp_send_500(req);
  }

  web_server->streaming_ = false;

  ESP_LOGI(TAG, "STREAM: closed. Frames: %u", frames);

  return res;
}

esp_err_t WebServer::snapshot_handler_(struct httpd_req *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;

  auto web_server = (WebServer*)req->user_ctx;

  res = httpd_resp_set_type(req, "image/jpeg");
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to set HTTP response type");
    return res;
  }

  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  web_server->snapshot_ = true;

  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->request_image();
  }

  std::shared_ptr<esphome::esp32_camera::CameraImage> image;
  image.swap(web_server->snapshot_image_);

  if (!image) {
    // retry as we might still be fetching image
    xSemaphoreTake(web_server->snapshot_semaphore_, IMAGE_REQUEST_TIMEOUT / portTICK_PERIOD_MS);
    image.swap(web_server->snapshot_image_);
  }
  if (!image) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to acquire frame");
    httpd_resp_send_500(req);
    res = ESP_FAIL;
  }
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Content-Length", esphome::to_string(image->get_data_length()).c_str());
  }
  if (res == ESP_OK) {
    res = httpd_resp_send(req, (const char *)image->get_data_buffer(), image->get_data_length());
  }

  web_server->snapshot_ = false;
  return res;
}

}  // namespace esp32_camera_web_server2
}  // namespace esphome

#endif
