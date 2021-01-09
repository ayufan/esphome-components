#pragma once

// const uint16_t PROP_WRITE_HANDLE = 0x411;
// const uint16_t PROP_NTFY_HANDLE = 0x421;

#include <esp_bt_defs.h>

const esp_bt_uuid_t PROP_SERVICE_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = {
    .uuid128 = {      
      0x67, 0xdf, 0xd1, 0x30, 0x42, 0x16, 
      0x39, 0x89,
      0xe4, 0x11,
      0xe9, 0x47,
      0x00, 0xee, 0xe9, 0x47
    }
  }
};

const esp_bt_uuid_t PROP_PIN_CHARACTERISTIC_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = { // 47e9ee30-47e9-11e4-8939-164230d1df67
    .uuid128 = { 
      0x67, 0xdf, 0xd1, 0x30, 0x42, 0x16, 
      0x39, 0x89,
      0xe4, 0x11,
      0xe9, 0x47,
      0x30, 0xee, 0xe9, 0x47
    }
  }
};

const esp_bt_uuid_t PROP_TIME_CHARACTERISTIC_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = {  // 47e9ee01-47e9-11e4-8939-164230d1df67
    .uuid128 = { 
      0x67, 0xdf, 0xd1, 0x30, 0x42, 0x16, 
      0x39, 0x89,
      0xe4, 0x11,
      0xe9, 0x47,
      0x01, 0xee, 0xe9, 0x47
    }
  }
};

const esp_bt_uuid_t PROP_TEMPERATURE_CHARACTERISTIC_UUID = {
.len = ESP_UUID_LEN_128,
  .uuid = {  // 47e9ee2b-47e9-11e4-8939-164230d1df67
    .uuid128 = { 
      0x67, 0xdf, 0xd1, 0x30, 0x42, 0x16, 
      0x39, 0x89,
      0xe4, 0x11,
      0xe9, 0x47,
      0x2b, 0xee, 0xe9, 0x47
    }
  }
};

const esp_bt_uuid_t PROP_FLAGS_CHARACTERISTIC_UUID = {
.len = ESP_UUID_LEN_128,
  .uuid = {  // 47e9ee2a-47e9-11e4-8939-164230d1df67
    .uuid128 = { 
      0x67, 0xdf, 0xd1, 0x30, 0x42, 0x16, 
      0x39, 0x89,
      0xe4, 0x11,
      0xe9, 0x47,
      0x2a, 0xee, 0xe9, 0x47
    }
  }
};

const float COMETBLUEBT_AWAY_TEMP = 12.0;
const float COMETBLUEBT_MIN_TEMP = 8.0;
const float COMETBLUEBT_MAX_TEMP = 29.5;
const float COMETBLUEBT_OFF_TEMP = 0.0;
const float COMETBLUEBT_ON_TEMP = 30.0;