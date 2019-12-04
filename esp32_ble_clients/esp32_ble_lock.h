#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

const int ESP32_LOCK_TRY = 0;
const int ESP32_LOCK_WAIT = portMAX_DELAY;

#define LOCK_LOG(fmt, args...) // BLE_LOGD("esp32_ble_lock", fmt, ##args)

class ESP32BLELock
{
public:
  ESP32BLELock(SemaphoreHandle_t new_handle, int new_timeout = ESP32_LOCK_WAIT) {
    handle = new_handle;
    LOCK_LOG("LOCK::xSemaphoreTake: %p", handle);
    obtained = xSemaphoreTake(handle, new_timeout);
    LOCK_LOG("LOCK::xSemaphoreTaken? %p => %d", handle, obtained);
  }

  ESP32BLELock(ESP32BLELock &lock) {
    handle = lock.handle;
    obtained = lock.obtained;
    lock.obtained = false;
  }

  ~ESP32BLELock() {
    give();
  }

  operator bool () const {
    return obtained;
  }

  bool operator ! () const {
    return !obtained;
  }

public:
  bool wait(int timeout = portMAX_DELAY) {
    if (!obtained) {
      return false;
    }

    LOCK_LOG("LOCK::wait: xSemaphoreTake: %p", handle);
    auto ret = xSemaphoreTake(handle, timeout);
    LOCK_LOG("LOCK::wait: xSemaphoreTaken? %p => %d", handle, ret);
    return ret;
  }

  bool take(int timeout = portMAX_DELAY) {
    if (obtained) {
      return false;
    }

    LOCK_LOG("LOCK::take: xSemaphoreTake: %p", handle);
    obtained = xSemaphoreTake(handle, timeout);
    LOCK_LOG("LOCK::take: xSemaphoreTaken? %p => %d", handle, obtained);
    return obtained;;
  }

  bool give() {
    if (obtained) {
      LOCK_LOG("LOCK::give: xSemaphoreGive: %p", handle);
      xSemaphoreGive(handle);
      obtained = false;
      return true;
    }

    return false;
  }

private:
  SemaphoreHandle_t handle;
  bool obtained;

public:
  static bool take(SemaphoreHandle_t new_handle, int timeout = ESP32_LOCK_WAIT) {
    return xSemaphoreGive(new_handle);
  }

  static void give(SemaphoreHandle_t new_handle) {
    xSemaphoreGive(new_handle);
  }
};
