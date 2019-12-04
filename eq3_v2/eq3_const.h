#pragma once

// const uint16_t PROP_WRITE_HANDLE = 0x411;
// const uint16_t PROP_NTFY_HANDLE = 0x421;

#include <esp_bt_defs.h>

const esp_bt_uuid_t PROP_SERVICE_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = {
    .uuid128 = {
      0x46, 0x70, 0xb7, 0x5b, 0xff, 0xa6,
      0x4a, 0x13,
      0x90, 0x90,
      0x4f, 0x65,
      0x42, 0x51, 0x13, 0x3e
    }
  }
};

const esp_bt_uuid_t PROP_COMMAND_CHARACTERISTIC_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = {
    .uuid128 = {
      0x09, 0xea, 0x79, 0x81, 0xdf, 0xb8,
      0x4b, 0xdb,
      0xad, 0x3b,
      0x4a, 0xce,
      0x5a, 0x58, 0xa4, 0x3f
    }
  }
};

const esp_bt_uuid_t PROP_NOTIFY_CHARACTERISTIC_UUID = {
  .len = ESP_UUID_LEN_128,
  .uuid = {
    .uuid128 = {
      0x2a, 0xeb, 0xe0, 0xf4, 0x90, 0x6c,
      0x41, 0xaf,
      0x96, 0x09,
      0x29, 0xcd,
      0x4d, 0x43, 0xe8, 0xd0
    }
  }
};

const uint8_t PROP_ID_QUERY = 0;
const uint8_t PROP_ID_RETURN = 1;
const uint8_t PROP_INFO_QUERY = 3;
const uint8_t PROP_INFO_RETURN = 2;
const uint8_t PROP_COMFORT_ECO_CONFIG = 0x11;
const uint8_t PROP_OFFSET = 0x13;
const uint8_t PROP_WINDOW_OPEN_CONFIG = 0x14;
const uint8_t PROP_SCHEDULE_QUERY = 0x20;
const uint8_t PROP_SCHEDULE_RETURN = 0x21;

const uint8_t PROP_MODE_WRITE = 0x40;
const uint8_t PROP_TEMPERATURE_WRITE = 0x41;
const uint8_t PROP_COMFORT = 0x43;
const uint8_t PROP_ECO = 0x44;
const uint8_t PROP_BOOST = 0x45;
const uint8_t PROP_LOCK = 0x80;

const float EQ3BT_AWAY_TEMP = 12.0;
const float EQ3BT_MIN_TEMP = 5.0;
const float EQ3BT_MAX_TEMP = 29.5;
const float EQ3BT_OFF_TEMP = 4.5;
const float EQ3BT_ON_TEMP = 30.0;

struct DeviceModeFlags {
  // bool auto_mode : 1; // implit
  bool manual_mode : 1;
  bool away_mode : 1;
  bool boost_mode : 1;
  bool dst_mode : 1;
  bool window_mode : 1;
  bool locked_mode : 1;
  bool reserved1 : 1;
  bool low_battery : 1;
} __attribute__((packed));

struct DeviceStatePreset {
  uint8_t window_open_temp;
  uint8_t window_open_time;
  uint8_t comfort_temp;
  uint8_t eco_temp;
  uint8_t temp_offset;
} __attribute__((packed));

struct DeviceState {
  uint8_t cmd; // PROP_INFO_RETURN
  uint8_t subcmd; // 1
  DeviceModeFlags mode;
  uint8_t valve;
  uint8_t uknown1; // should be: 0x4
  uint8_t target_temp;
} __attribute__((packed));

struct DeviceStateExtended : DeviceState {
  uint8_t away[4]; // away time
  DeviceStatePreset presets;
};

