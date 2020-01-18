# ayufan's esphome custom components

This repository contains a collection of my custom components.

## 1. Installation

Clone this repository into `custom_components` in a folder
where the `config.yaml` is stored.

```bash
git clone https://github.com/ayufan/esphome-components.git custom_components
```

This is the directory structure expected:

```bash
$ ls -al
total 64
drwxr-xr-x 6 ayufan ayufan 4096 gru  3 22:56 .
drwxr-xr-x 9 ayufan ayufan 4096 gru  3 13:19 ..
drwxr-xr-x 7 ayufan ayufan 4096 gru  5 17:25 custom_components
-rw-r--r-- 1 ayufan ayufan 1509 gru  3 10:00 esp32-emeter.yaml
```

## 2. Components

### 2.1. `inode_ble`

A component to support [iNode.pl](https://inode.pl/) devices
(energy meters and magnetometers).

#### 2.1.1. Example

```yaml
wifi:
  ...
  power_save_mode: none

# the BT window is configured to be a small portion
# of interval to have a lot of time to handle WiFi
esp32_ble_tracker:
  scan_parameters:
    active: false
    interval: 288ms
    window: 144ms

# monitor RSSI and Power, Energy, and Battery Levels
# the `constant` defines a how many pulses are per unit
# ex.: 1500 pulses/kWh
# all sensors of inode_ble:
# - avg_w
# - avg_dm3
# - avg_raw
# - total_kwh
# - total_dm3
# - total_raw
# - battery_level
# - light_level
sensor:
  - platform: ble_rssi
    mac_address: 00:0b:57:36:df:51
    name: "Emeter RSSI"
    icon: mdi:wifi
    expire_after: 10min

  - platform: inode_ble
    mac_address: 00:0b:57:36:df:51
    constant: 1500
    avg_w:
      name: 'Emeter Power'
      expire_after: 10min
    total_kwh:
      name: 'Emeter Energy'
      expire_after: 10min
    battery_level:
      name: 'Emeter Battery Level'
      expire_after: 10min
```

### 2.2. `eq3_v2`

A component to support [eQ-3 Radiator Thermostat Model N](https://www.eq-3.com/products/homematic/detail/radiator-thermostat-model-n.html),
and maybe also other ones.

This uses custom `esp32_ble_clients` implementation to support
Bluetooth on ESP32.

#### 2.2.1. Stability

This is quite challenging to ensure that BT works well with WiFi.
Ideally it is preferred to run only `eq3_v2` component
on a single device, with all basic services. Usage of complex services,
or complex logic can cause stability issues.

I also noticed that extensive logging (like in `VERBOSE` mode)
during the BT connection cases WiFi to loose beacons,
and results in disconnect.

#### 2.2.2. Example

```yaml
wifi:
  ...
  power_save_mode: none

# time is required by `eq3_v2` to send
# an accurate time spec when requesting
# current state
time:
  - platform: sntp
    id: sntp_time

# refresh component state every 30mins,
# and announce it to Home Assistant MQTT
climate:
  - platform: eq3_v2
    id: office_eq3
    name: Office EQ3
    mac_address: 00:1A:22:12:5B:34
    update_interval: 30min
    valve: # optional, allows to see valve state in %
      name: Office EQ3 Valve State
      expire_after: 61min

# allow to force refresh component state
switch:
  - platform: template
    name: "Refresh Office EQ3"
    lambda: "return false;"
    turn_on_action:
      - component.update: office_eq3
```

### 2.3. `memory`

Simple component that periodically prints free memory of node.

```yaml
memory:
```
### 2.4 Comet Blue

```yaml
wifi:
  ssid: "..."
  password: "..."
  power_save_mode: none

debug:

# Enable logging
logger:
  level: DEBUG

# Enable Home Assistant API
api:

ota:

time:
  - platform: sntp
    id: sntp_time

climate:
  - platform: cometblue
    pin: 0000
    id: trv_cb1
    name: Radiator 1
    mac_address: 01:23:45:56:78:90
    update_interval: 5min
    temperature_offset: 0
    window_open_sensitivity: 4
    window_open_minutes: 10

```yaml

## 3. Author & License

Kamil Trzciński, MIT, 2019
