#pragma once

#if 1
#define BLE_LOGE(TAG, fmt, args...) \
  do { } while(0)

#define BLE_LOGW(TAG, fmt, args...) \
  do { } while(0)

#define BLE_LOGD(TAG, fmt, args...) \
  do { } while(0)

#define BLE_LOGI(TAG, fmt, args...) \
  do { } while(0)
#else
#define BLE_LOGE(TAG, fmt, args...) \
  printf("%s: " fmt, TAG, ## args)

#define BLE_LOGW(TAG, fmt, args...) \
  printf("%s: " fmt, TAG, ## args)

#define BLE_LOGD(TAG, fmt, args...) \
  printf("%s: " fmt, TAG, ## args)

#define BLE_LOGI(TAG, fmt, args...) \
  printf("%s: " fmt, TAG, ## args)
#endif
