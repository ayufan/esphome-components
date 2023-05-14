# ayufan's esphome custom components

This repository contains a collection of my custom components
for [ESPHome](https://esphome.io/).

## 1. Usage

Use latest [ESPHome](https://esphome.io/) (at least v1.18.0)
with external components and add this to your `.yaml` definition:

```yaml
external_components:
  - source: github://ayufan/esphome-components
```

## 2. Components

### 2.1. `inode_ble`

A component to support [iNode.pl](https://inode.pl/) devices
(energy meters and magnetometers).

#### 2.1.1. Example

```yaml
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

### 2.3. `tplink_plug`

This plugin allows to emulate TPLink HS100/HS110 type of plug
using LAN protocol with ESPHome. Especially useful where you
want to use existing software that supports these type of plugs,
but not others.

```yaml
tplink_plug:
  plugs:
    voltage: my_voltage
    current: my_current
    total: my_total
    state: relay

# Example config for Gosund SP111

sensor:
  - id: my_daily_total
    platform: total_daily_energy
    name: "MK3S+ Daily Energy"
    power_id: my_power

  - platform: hlw8012
    sel_pin:
      number: GPIO12
      inverted: true
    cf_pin: GPIO05
    cf1_pin: GPIO04
    current:
      id: my_current
      name: "MK3S+ Current"
      expire_after: 1min
    voltage:
      id: my_voltage
      name: "MK3S+ Voltage"
      expire_after: 1min
    power:
      id: my_power
      name: "MK3S+ Power"
      expire_after: 1min
      filters:
        - multiply: 0.5
    energy:
      id: my_total
      name: "MK3S+ Energy"
      expire_after: 1min
      filters:
        - multiply: 0.5
    change_mode_every: 3
    update_interval: 15s
    voltage_divider: 748
    current_resistor: 0.0012

    # {"PowerSetCal":10085}
    # {"VoltageSetCal":1581}
    # {"CurrentSetCal":3555}

binary_sensor:
  - platform: gpio
    pin:
      number: GPIO13
      mode: INPUT
      inverted: true
    name: "MK3S+ Button"
    on_press:
      - switch.toggle: relay

switch:
  - platform: gpio
    id: relay
    name: "MK3S+ Switch"
    pin: GPIO15
    restore_mode: RESTORE_DEFAULT_OFF
    icon: mdi:power-socket-eu
    on_turn_on:
      - output.turn_on: led
    on_turn_off:
      - output.turn_off: led

status_led:
  pin:
    number: GPIO00
    inverted: true

output:
  - platform: gpio
    pin: GPIO02
    inverted: true
    id: led
```

### 2.4. `memory`

Simple component that periodically prints free memory of node.

```yaml
memory:
```

### 2.5. `esp32_camera_web_server` (upstreamed)

_This component was removed as it is part of upstream esphome starting with version 2021.11: https://esphome.io/components/esp32_camera_web_server.html_

Simple component to expose `esp32_camera` via HTTP snapshot and stream interface:

- http://esphome:8080/
- http://esphome:8081/

```yaml
esp32_camera_web_server:
  # define only what is needed
  # only a single stream is supported at a given time
  - port: 8080
    mode: stream
  - port: 8081
    mode: snapshot

esp32_camera:
  name: My Camera
  external_clock:
    pin: GPIO0
    frequency: 20MHz
  i2c_pins:
    sda: GPIO26
    scl: GPIO27
  data_pins: [GPIO5, GPIO18, GPIO19, GPIO21, GPIO36, GPIO39, GPIO34, GPIO35]
  vsync_pin: GPIO25
  href_pin: GPIO23
  pixel_clock_pin: GPIO22
  power_down_pin: GPIO32
  resolution: 1600x1200
  jpeg_quality: 12
```

### 2.6. `zp16` sensor

The ZP16 is a low-cost TVOC sensor.

Links:
- https://www.winsentech.com/npublic/opdfjs/web/viewer.html?u=1621050968327720960&fileName=ZP16%20Air-Quality%20Detection%20Module%20manual%201.1
- https://www.aliexpress.com/i/4000068334696.html

```yaml
uart:
  - id: zp16_uart
    rx_pin: D7
    tx_pin: D6
    baud_rate: 9600

sensor:
  - platform: zp16
    tvoc:
      name: "${prefix} TVOC ZP16"
    tvoc_scale:
      name: "${prefix} TVOC Scale ZP16"
    uart_id: zp16_uart
    # rx_only: true
    # update_interval: 5s # use Q&A mode and request update every 5s
    # update_interval: 0 # use Active-Upload mode and wait for periodic update (~1s)
```

## 3. Author & License

Kamil Trzciński, MIT, 2019-2021
