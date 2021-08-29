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

struct DeviceTemp {
  uint8_t device;

  DeviceTemp(float temp) { device = temp * 2.0; }
  operator float() { return to_user(); }
  float to_user() const { return device / 2.0f; }
  uint8_t to_device() const { return device; }
  bool valid() const { return EQ3BT_OFF_TEMP <= to_user() && to_user() <= EQ3BT_ON_TEMP; }
} __attribute__((packed));

struct DeviceTempOffset { 
  uint8_t device;

  DeviceTempOffset(float temp) { device = (temp + 3.5) * 2.0; }
  operator float() { return to_user(); }
  float to_user() const { return device / 2.0f - 3.5; }
  uint8_t to_device() const { return device; }
  bool valid() const { return -3.5 <= to_user() && to_user() <= 3.5; }
} __attribute__((packed));

struct DeviceTime {
  uint8_t device;

  DeviceTime(uint8_t hour, uint8_t minute) { device = hour * 6 + minute / 10; }
  uint8_t to_hour() const { return (device * 10) / 60; }
  uint8_t to_minute() const { return (device * 10) % 60; }
  uint8_t to_device() const { return device; }
  bool valid() const { return device <= 24 * 6; }
} __attribute__((packed));

struct DeviceWindowOpenTime {
  uint8_t device;

  DeviceWindowOpenTime(uint8_t minutes) { device = minutes / 5; }
  uint8_t to_minutes() const { return device * 5; }
  uint8_t to_device() const { return device; }
  bool valid() const { return to_minutes() <= 60; }
} __attribute__((packed));

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

  std::string to_string() const {
    std::string mode;

    if (manual_mode)
      mode += " manual";
    if (away_mode)
      mode += " away";
    if (boost_mode)
      mode += " boost";
    if (dst_mode)
      mode += " dst";
    if (window_mode)
      mode += " window";
    if (locked_mode)
      mode += " locked";
    if (low_battery)
      mode += " low";

    if (mode.empty()) {
      return mode;
    }

    return mode.substr(1); // remove first space
  }
} __attribute__((packed));

struct DeviceAwayTime {
  uint8_t day;
  uint8_t year;
  uint8_t hour;
  uint8_t month;

  uint8_t to_year() const { return year + 2000; }
  uint8_t to_month() const { return month; }
  uint8_t to_day() const { return day; }
  uint8_t to_hour() const { return hour / 2; }
  uint8_t to_minute() const { return hour & 1 ? 30 : 0; }

  bool valid() const {
    return to_year() >= 2019 &&
      to_year() < 2030 &&
      1 <= to_day() && to_day() <= 31 &&
      to_hour() < 24 &&
      to_minute() < 60 &&
      1 <= to_month() && 12 <= to_month();
  }
} __attribute__((packed));

struct DeviceStateReturn {
  uint8_t cmd; // PROP_INFO_RETURN
  uint8_t subcmd; // 1
  DeviceModeFlags mode;
  uint8_t valve;
  uint8_t uknown1; // should be: 0x4
  DeviceTemp target_temp;
  DeviceAwayTime away;
  DeviceTemp window_open_temp;
  DeviceWindowOpenTime window_open_time;
  DeviceTemp comfort_temp;
  DeviceTemp eco_temp;
  DeviceTempOffset temp_offset;
} __attribute__((packed));

struct DeviceStateReturnExtended : DeviceStateReturn {
} __attribute__((packed));

struct DeviceIDReturn {
  uint8_t cmd; // PROP_ID_RETURN
  uint8_t version;
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t serial[10];
  uint8_t unknown3;
} __attribute__((packed));

struct DeviceSchedule {
  DeviceTemp temp;
  DeviceTime till;

  bool valid() const { return temp.valid() && till.valid(); }
} __attribute__((packed));

struct DeviceScheduleReturn {
  uint8_t cmd; // PROP_SCHEDULE_RETURN
  uint8_t day;
  DeviceSchedule hours[0];

  int hours_count(size_t data_size) const {
    if (data_size < sizeof(DeviceScheduleReturn)) {
      return 0;
    }

    return (data_size - sizeof(DeviceScheduleReturn)) / sizeof(DeviceSchedule);
  }
} __attribute__((packed));
